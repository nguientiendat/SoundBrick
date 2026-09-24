#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <glob.h>
#include <SDL2/SDL.h>
#include "soundcloud/SoundCloudClient.h"
#include "ui/TextRenderer.h"
#include "network/HttpClient.h"
#include <thread>
#include <atomic>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

// Download image from URL, convert to Texture, and calculate dominant color
SDL_Texture* loadTextureFromUrl(SDL_Renderer* ren, const std::string& url, SDL_Color& outBgColor) {
    if (url.empty()) return nullptr;
    std::string bigUrl = url;
    size_t pos = bigUrl.find("-large.jpg");
    if (pos != std::string::npos) {
        bigUrl.replace(pos, 10, "-t500x500.jpg");
    }
    
    HttpClient http;
    HttpResponse res = http.get(bigUrl);
    if (res.statusCode != 200 || res.body.empty()) return nullptr;
    
    int w, h, channels;
    unsigned char* data = stbi_load_from_memory((const stbi_uc*)res.body.data(), res.body.size(), &w, &h, &channels, 4);
    if (!data) return nullptr;
    
    // --- Calculate artistic background color (Average Color) ---
    long long r = 0, g = 0, b = 0;
    int pixelCount = w * h;
    int step = 10; // Sample 1/10 of pixels for extreme speed
    int samples = 0;
    for (int i = 0; i < pixelCount; i += step) {
        r += data[i*4 + 0];
        g += data[i*4 + 1];
        b += data[i*4 + 2];
        samples++;
    }
    // Reduce brightness by 60% to make white text pop
    outBgColor.r = (r / samples) * 0.4f;
    outBgColor.g = (g / samples) * 0.4f;
    outBgColor.b = (b / samples) * 0.4f;
    outBgColor.a = 255;
    
    // --- Ultra-smooth Rounded Corners Algorithm ---
    int R = 30; // Corner radius (30 pixels)
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int cx = -1, cy = -1;
            // Determine the center of the 4 circles at the corners
            if (x < R && y < R) { cx = R; cy = R; } // Top-left
            else if (x > w - 1 - R && y < R) { cx = w - 1 - R; cy = R; } // Top-right
            else if (x < R && y > h - 1 - R) { cx = R; cy = h - 1 - R; } // Bottom-left
            else if (x > w - 1 - R && y > h - 1 - R) { cx = w - 1 - R; cy = h - 1 - R; } // Bottom-right
            
            if (cx != -1 && cy != -1) {
                float dx = (float)(x - cx);
                float dy = (float)(y - cy);
                float dist = std::sqrt(dx*dx + dy*dy);
                
                // If distance > radius -> Outside rounded corner -> Erase pixel (Alpha = 0)
                if (dist > R) {
                    data[(y * w + x) * 4 + 3] = 0;
                } else if (dist > R - 1.0f) {
                    // Edge of the curve -> Apply Anti-aliasing for smoothness
                    float alpha = 1.0f - (dist - (R - 1.0f));
                    data[(y * w + x) * 4 + 3] = (unsigned char)(255 * alpha);
                }
            }
        }
    }
    
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom(data, w, h, 32, w*4, SDL_PIXELFORMAT_RGBA32);
    SDL_Texture* tex = nullptr;
    if (surf) {
        tex = SDL_CreateTextureFromSurface(ren, surf);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND); // Enable Alpha blending
        SDL_FreeSurface(surf);
    }
    stbi_image_free(data);
    return tex;
}

// Utility to draw beautiful glassmorphism rounded rectangles
void fillRoundedRect(SDL_Renderer* ren, int x, int y, int w, int h, int r, SDL_Color color) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);
    
    SDL_Rect mid = {x, y + r, w, h - 2*r};
    SDL_RenderFillRect(ren, &mid);
    SDL_Rect top = {x + r, y, w - 2*r, r};
    SDL_RenderFillRect(ren, &top);
    SDL_Rect bot = {x + r, y + h - r, w - 2*r, r};
    SDL_RenderFillRect(ren, &bot);
    
    for (int dy = 0; dy < r; dy++) {
        int dx = (int)std::sqrt(r*r - (r - dy)*(r - dy));
        SDL_RenderDrawLine(ren, x + r - dx, y + dy, x + r, y + dy); // Top-Left
        SDL_RenderDrawLine(ren, x + w - r - 1, y + dy, x + w - r - 1 + dx, y + dy); // Top-Right
        SDL_RenderDrawLine(ren, x + r - dx, y + h - 1 - dy, x + r, y + h - 1 - dy); // Bottom-Left
        SDL_RenderDrawLine(ren, x + w - r - 1, y + h - 1 - dy, x + w - r - 1 + dx, y + h - 1 - dy); // Bottom-Right
    }
}

// Utility function to stop music
void stopMusic() {
    system("killall -9 tplayerdemo >/dev/null 2>&1");
    system("killall -9 tail >/dev/null 2>&1");
}

// Background download state
std::atomic<bool> isDownloading{false};
char downloadStatusText[128] = "";

void downloadTrackTask(soundcloud::SoundCloudClient* sc, soundcloud::Track track) {
    isDownloading = true;
    strcpy(downloadStatusText, "Downloading to SD Card...");
    
    std::string streamUrl = sc->resolveStreamUrl(track.permalink);
    if (streamUrl.empty()) {
        strcpy(downloadStatusText, "Failed to resolve URL!");
        SDL_Delay(2000);
        isDownloading = false;
        return;
    }

    std::string filename = track.title;
    for (char& c : filename) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
    }
    
    system("mkdir -p /mnt/SDCARD/Music/SoundBrick/ 2>/dev/null");
    std::string filepath = "/mnt/SDCARD/Music/SoundBrick/" + filename + ".mp3";
    
    HttpClient http;
    if (http.downloadFile(streamUrl, filepath)) {
        strcpy(downloadStatusText, "Saved to /Music/SoundBrick/ !");
    } else {
        strcpy(downloadStatusText, "Download Failed!");
    }
    
    SDL_Delay(3000);
    isDownloading = false;
}

enum AppState { STATE_MAIN_MENU, STATE_SEARCH, STATE_LOADING, STATE_LIST, STATE_NOW_PLAYING };

int main(int argc, char* argv[]) {
    std::cout << "Starting SoundBrick UI..." << std::endl;

    soundcloud::SoundCloudClient sc;
    if (!sc.loadConfig("data/settings.json")) {
        std::cout << "Warning: Could not open data/settings.json!" << std::endl;
    }
    std::vector<soundcloud::Track> tracks;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* win = SDL_CreateWindow("SoundBrick", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 768, SDL_WINDOW_SHOWN);
    if (!win) return 1;

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) return 1;

    SDL_GameController* controller = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            break;
        }
    }

    TextRenderer textRenderer(ren);
    textRenderer.loadFont("data/fonts/Roboto-Regular.ttf", 32.0f);
    TextRenderer largeText(ren);
    largeText.loadFont("data/fonts/Roboto-Regular.ttf", 48.0f);
    TextRenderer smallText(ren);
    smallText.loadFont("data/fonts/Roboto-Regular.ttf", 18.0f);

    // Load Background Image
    SDL_Texture* bgTexture = nullptr;
    int bgW, bgH, bgChannels;
    unsigned char* bgData = stbi_load("data/background.jpg", &bgW, &bgH, &bgChannels, 4);
    if (bgData) {
        SDL_Surface* bgSurf = SDL_CreateRGBSurfaceWithFormatFrom(bgData, bgW, bgH, 32, bgW * 4, SDL_PIXELFORMAT_RGBA32);
        if (bgSurf) {
            bgTexture = SDL_CreateTextureFromSurface(ren, bgSurf);
            SDL_FreeSurface(bgSurf);
        }
        stbi_image_free(bgData);
    } else {
        std::cout << "Note: data/background.jpg not found, falling back to colors." << std::endl;
    }

    AppState state = STATE_MAIN_MENU;
    int mainMenuIndex = 0;
    bool isOfflineMode = false;
    std::string searchQuery = "lofi"; 
    
    const char* keyboard[40] = {
        "a","b","c","d","e","f","g","h","i","j",
        "k","l","m","n","o","p","q","r","s","t",
        "u","v","w","x","y","z","0","1","2","3",
        "4","5","6","7","8","9","DEL","SPC","CLR","GO"
    };
    int kbIndex = 0;

    int selectedIndex = 0;
    int playingIndex = -1;
    bool isRunning = true;
    SDL_Event e;
    
    bool needLoadArtwork = false;
    SDL_Texture* currentArtwork = nullptr;
    SDL_Color currentBgColor = {30, 25, 35, 255}; // Gradient background color
    
    // Playback and progress bar variables
    Uint32 playStartTime = 0;
    Uint32 currentPlaybackTime = 0;
    bool isPlaying = false;
    int nowPlayingSelectedBtn = 1; // 0=Prev, 1=Play, 2=Next

    auto formatTime = [](int ms) {
        if (ms < 0) ms = 0;
        int sec = ms / 1000;
        int min = sec / 60;
        sec %= 60;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%02d", min, sec);
        return std::string(buf);
    };

    // Automatically scan for system backlight control file path
    auto getBacklightPath = []() {
        glob_t glob_result;
        glob("/sys/class/backlight/*/brightness", GLOB_TILDE, NULL, &glob_result);
        std::string path = "";
        if (glob_result.gl_pathc > 0) {
            path = glob_result.gl_pathv[0];
        }
        globfree(&glob_result);
        return path;
    };

    std::string backlightPath = getBacklightPath();
    int savedBrightness = 100;
    bool isScreenOff = false;
    bool isConfirmingExit = false;

    while (isRunning) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                isRunning = false;
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) isRunning = false;
            } else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                int btn = e.cbutton.button;
                
                // Wake up screen if it's off (any button)
                if (isScreenOff) {
                    isScreenOff = false;
                    system("echo 0 > /sys/class/graphics/fb0/blank 2>/dev/null");
                    if (!backlightPath.empty()) {
                        std::string cmd = "echo " + std::to_string(savedBrightness) + " > " + backlightPath + " 2>/dev/null";
                        system(cmd.c_str());
                    }
                    continue;
                }
                
                // Xử lý Popup xác nhận thoát
                if (isConfirmingExit) {
                    if (btn == SDL_CONTROLLER_BUTTON_B) { // A button (Right) -> Yes
                        isRunning = false;
                    } else if (btn == SDL_CONTROLLER_BUTTON_A) { // B button (Down) -> No
                        isConfirmingExit = false;
                    }
                    continue; // Bỏ qua các thao tác phím khác khi đang hiện popup
                }

                // Press SELECT (BACK) to turn off screen (Battery Saver)
                if (btn == SDL_CONTROLLER_BUTTON_BACK) {
                    isScreenOff = true;
                    system("echo 1 > /sys/class/graphics/fb0/blank 2>/dev/null"); // Standard Linux screen blank
                    if (!backlightPath.empty()) {
                        FILE* f = fopen(backlightPath.c_str(), "r");
                        if (f) { fscanf(f, "%d", &savedBrightness); fclose(f); }
                        if (savedBrightness <= 0) savedBrightness = 100;
                        
                        std::string cmd = "echo 0 > " + backlightPath + " 2>/dev/null";
                        system(cmd.c_str());
                    }
                    continue;
                }

                if (btn == SDL_CONTROLLER_BUTTON_START) {
                    if (!isConfirmingExit) {
                        isConfirmingExit = true;
                    }
                }

                if (state == STATE_MAIN_MENU) {
                    if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP) {
                        mainMenuIndex--;
                        if (mainMenuIndex < 0) mainMenuIndex = 2;
                    }
                    if (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                        mainMenuIndex++;
                        if (mainMenuIndex > 2) mainMenuIndex = 0;
                    }
                    if (btn == SDL_CONTROLLER_BUTTON_B) { // A button (Right) -> Select
                        if (mainMenuIndex == 0) {
                            if (sc.getClientId().empty()) {
                                SDL_SetRenderDrawColor(ren, 30, 25, 35, 255);
                                SDL_RenderClear(ren);
                                textRenderer.drawText("Fetching initial API Key...", 300, 350, {255, 85, 0, 255});
                                SDL_RenderPresent(ren);
                                
                                std::string newId = sc.autoFetchClientId();
                                if (!newId.empty()) {
                                    sc.setClientId(newId);
                                    sc.saveConfig("data/settings.json");
                                }
                            }
                            state = STATE_SEARCH;
                            isOfflineMode = false;
                        } 
                        else if (mainMenuIndex == 1) {
                            // Offline Mode: Scan Directory
                            isOfflineMode = true;
                            tracks.clear();
                            glob_t glob_result;
                            glob("/mnt/SDCARD/Music/SoundBrick/*.mp3", 0, NULL, &glob_result);
                            for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
                                soundcloud::Track t;
                                std::string path = glob_result.gl_pathv[i];
                                size_t lastSlash = path.find_last_of("/");
                                size_t extPos = path.find(".mp3");
                                std::string filename = path.substr(lastSlash + 1, extPos - lastSlash - 1);
                                
                                t.title = filename;
                                t.artist = "Local File";
                                t.permalink = path; // Reuse permalink for local path
                                t.durationMs = 0; // We don't parse duration of local MP3 easily here
                                t.artworkUrl = ""; // No art for offline yet
                                tracks.push_back(t);
                            }
                            globfree(&glob_result);
                            selectedIndex = 0;
                            state = STATE_LIST;
                        }
                        else if (mainMenuIndex == 2) {
                            SDL_SetRenderDrawColor(ren, 30, 25, 35, 255);
                            SDL_RenderClear(ren);
                            textRenderer.drawText("Refreshing API Key...", 350, 350, {255, 85, 0, 255});
                            SDL_RenderPresent(ren);
                            
                            std::string newId = sc.autoFetchClientId();
                            if (!newId.empty()) {
                                sc.setClientId(newId);
                                sc.saveConfig("data/settings.json");
                            }
                        }
                    }
                }
                else if (state == STATE_SEARCH) {
                    if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP) kbIndex -= 10;
                    if (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN) kbIndex += 10;
                    if (btn == SDL_CONTROLLER_BUTTON_DPAD_LEFT) kbIndex--;
                    if (btn == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) kbIndex++;
                    
                    if (kbIndex < 0) kbIndex += 40;
                    if (kbIndex >= 40) kbIndex -= 40;

                    if (btn == SDL_CONTROLLER_BUTTON_B) { // TrimUI A button
                        std::string key = keyboard[kbIndex];
                        std::string& targetStr = searchQuery;
                        
                        if (key == "DEL") {
                            if (!targetStr.empty()) targetStr.pop_back();
                        } else if (key == "SPC") {
                            targetStr += " ";
                        } else if (key == "CLR") {
                            targetStr = "";
                        } else if (key == "GO") {
                            if (!searchQuery.empty()) state = STATE_LOADING;
                        } else {
                            if (targetStr.length() < 32) {
                                targetStr += key;
                            }
                        }
                    } else if (btn == SDL_CONTROLLER_BUTTON_A) { // TrimUI B button
                        if (!searchQuery.empty()) searchQuery.pop_back();
                        else state = STATE_MAIN_MENU; // Exit search
                    }
                } 
                else if (state == STATE_LIST) {
                    if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP) selectedIndex--;
                    if (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN) selectedIndex++;
                    
                    if (btn == SDL_CONTROLLER_BUTTON_B) { // A button (Right)
                        if (!tracks.empty() && selectedIndex >= 0 && selectedIndex < (int)tracks.size()) {
                            if (playingIndex != selectedIndex) {
                                playingIndex = selectedIndex;
                                needLoadArtwork = true;
                            }
                            state = STATE_NOW_PLAYING;
                        }
                    } else if (btn == SDL_CONTROLLER_BUTTON_A) { // B button (Down)
                        state = STATE_MAIN_MENU;
                    }
                }
                else if (state == STATE_NOW_PLAYING) {
                    if (btn == SDL_CONTROLLER_BUTTON_A) { // B button (Down) back to List
                        state = STATE_LIST;
                    }
                    else if (btn == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
                        nowPlayingSelectedBtn--;
                        if (nowPlayingSelectedBtn < 0) nowPlayingSelectedBtn = 0;
                    }
                    else if (btn == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                        nowPlayingSelectedBtn++;
                        if (nowPlayingSelectedBtn > 2) nowPlayingSelectedBtn = 2;
                    }
                    else if (btn == SDL_CONTROLLER_BUTTON_B) { // A button (Right) to interact with media controls
                        if (nowPlayingSelectedBtn == 0) { // Previous button
                            if (playingIndex > 0) {
                                playingIndex--;
                                needLoadArtwork = true;
                            }
                        } else if (nowPlayingSelectedBtn == 2) { // Next button
                            if (playingIndex < (int)tracks.size() - 1) {
                                playingIndex++;
                                needLoadArtwork = true;
                            }
                        } else if (nowPlayingSelectedBtn == 1) { // Toggle Play/Pause
                            isPlaying = !isPlaying;
                            if (isPlaying) {
                                // Resume via SIGCONT signal
                                system("killall -CONT tplayerdemo >/dev/null 2>&1");
                                playStartTime = SDL_GetTicks() - currentPlaybackTime;
                            } else {
                                // Pause via SIGSTOP signal
                                system("killall -STOP tplayerdemo >/dev/null 2>&1");
                                currentPlaybackTime = SDL_GetTicks() - playStartTime;
                            }
                        }
                    }
                    else if (btn == SDL_CONTROLLER_BUTTON_X) { // Y button (Download)
                        if (!isDownloading) {
                            std::thread(downloadTrackTask, &sc, tracks[playingIndex]).detach();
                        }
                    }
                }
            }
        }

        // --- UPDATE LOGIC ---
        if (state == STATE_LOADING) {
            SDL_SetRenderDrawColor(ren, 18, 18, 18, 255);
            SDL_RenderClear(ren);
            largeText.drawText("Searching: " + searchQuery + " ...", 100, 350, {255, 85, 0, 255});
            SDL_RenderPresent(ren);
            
            tracks = sc.searchTracks(searchQuery, 50);
            selectedIndex = 0;
            state = STATE_LIST;
            continue;
        }
        
        if (state == STATE_NOW_PLAYING && needLoadArtwork) {
            SDL_SetRenderDrawColor(ren, 25, 20, 30, 255);
            SDL_RenderClear(ren);
            largeText.drawText("Loading Artwork...", 350, 350, {255, 85, 0, 255});
            SDL_RenderPresent(ren);
            
            stopMusic();
            std::string streamUrl = "";
            if (isOfflineMode) {
                streamUrl = tracks[playingIndex].permalink; // Absolute path to MP3
            } else {
                streamUrl = sc.resolveStreamUrl(tracks[playingIndex].permalink);
            }
            
            if (!streamUrl.empty()) {
                std::string cmd = "tail -f /dev/null | tplayerdemo \"" + streamUrl + "\" >/dev/null 2>&1 &";
                system(cmd.c_str());
            }
            
            if (currentArtwork) {
                SDL_DestroyTexture(currentArtwork);
                currentArtwork = nullptr;
            }
            currentArtwork = loadTextureFromUrl(ren, tracks[playingIndex].artworkUrl, currentBgColor);
            needLoadArtwork = false;
            
            // Reset playback time
            currentPlaybackTime = 0;
            playStartTime = SDL_GetTicks();
            isPlaying = true;
            nowPlayingSelectedBtn = 1;
            continue;
        }
        
        // Simulate auto-advancing progress bar
        if (state == STATE_NOW_PLAYING && isPlaying) {
            currentPlaybackTime = SDL_GetTicks() - playStartTime;
            if (currentPlaybackTime > (Uint32)tracks[playingIndex].durationMs && tracks[playingIndex].durationMs > 0) {
                // Auto-skip to next track when finished (Loop back to start if at end)
                playingIndex++;
                if (playingIndex >= (int)tracks.size()) playingIndex = 0;
                needLoadArtwork = true;
            }
        }
        
        if (state == STATE_LIST && !tracks.empty()) {
            if (selectedIndex < 0) selectedIndex = tracks.size() - 1;
            if (selectedIndex >= (int)tracks.size()) selectedIndex = 0;
        }

        // --- RENDER ---
        if (isScreenOff) {
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);
            SDL_RenderPresent(ren);
            SDL_Delay(100); // Sleep longer to maximize CPU rest
            continue;
        }

        if (state == STATE_MAIN_MENU) {
            if (bgTexture) {
                SDL_RenderCopy(ren, bgTexture, NULL, NULL);
                // Lớp phủ đen mờ để làm nổi bật chữ
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren, 0, 0, 0, 120);
                SDL_Rect fullScreen = {0, 0, 1024, 768};
                SDL_RenderFillRect(ren, &fullScreen);
            } else {
                // Cool artistic gradient for main menu if no image
                for (int i = 0; i < 768; i++) {
                    float t = (float)i / 768.0f;
                    SDL_SetRenderDrawColor(ren, (Uint8)(30 * (1.0f - t) + 15), (Uint8)(25 * (1.0f - t) + 10), (Uint8)(35 * (1.0f - t) + 18), 255);
                    SDL_RenderDrawLine(ren, 0, i, 1024, i);
                }
            }
            
            largeText.drawText("SOUNDBRICK", 320, 80, {255, 85, 0, 255});
            smallText.drawText("THE RETRO CLOUD PLAYER", 380, 150, {150, 150, 150, 255});
            
            const char* options[] = {"1. SEARCH ONLINE", "2. OFFLINE LIBRARY", "3. REFRESH API KEY"};
            int startY = 300;
            for (int i = 0; i < 3; i++) {
                if (i == mainMenuIndex) {
                    fillRoundedRect(ren, 262, startY + i * 100, 500, 70, 15, {255, 85, 0, 180}); // Glass Orange
                    largeText.drawText(options[i], 310, startY + i * 100 + 10, {255, 255, 255, 255});
                } else {
                    fillRoundedRect(ren, 262, startY + i * 100, 500, 70, 15, {30, 30, 30, 120}); // Glass Black
                    textRenderer.drawText(options[i], 310, startY + i * 100 + 15, {200, 200, 200, 255});
                }
            }
            textRenderer.drawText("D-PAD: Move | A: Select | START: Exit | SELECT: Screen Off", 50, 720, {150, 150, 150, 255});
        }
        else if (state == STATE_NOW_PLAYING) {
            // Draw highly artistic Gradient from dominant color to dark gray
            for (int i = 0; i < 768; i++) {
                float t = (float)i / 768.0f;
                // t^2 function for a smooth curve, making the bottom pitch black
                float factor = t * t; 
                Uint8 r = currentBgColor.r * (1.0f - factor) + 18 * factor;
                Uint8 g = currentBgColor.g * (1.0f - factor) + 18 * factor;
                Uint8 b = currentBgColor.b * (1.0f - factor) + 18 * factor;
                SDL_SetRenderDrawColor(ren, r, g, b, 255);
                SDL_RenderDrawLine(ren, 0, i, 1024, i);
            }
        } 
        else {
            if (bgTexture && (state == STATE_LIST || state == STATE_SEARCH || state == STATE_LOADING)) {
                SDL_RenderCopy(ren, bgTexture, NULL, NULL);
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren, 0, 0, 0, 180); // Overlay tối hơn chút để đọc List/Search dễ hơn
                SDL_Rect fullScreen = {0, 0, 1024, 768};
                SDL_RenderFillRect(ren, &fullScreen);
            } else {
                SDL_SetRenderDrawColor(ren, 18, 18, 18, 255);
                SDL_RenderClear(ren);
            }
        }

        if (state == STATE_SEARCH) {
            largeText.drawText("SOUNDBRICK SEARCH", 50, 50, {255, 85, 0, 255});
            
            fillRoundedRect(ren, 50, 150, 924, 60, 15, {40, 40, 40, 180}); // Glass Input Box
            
            std::string displayStr = searchQuery + "_";
            largeText.drawText(displayStr, 70, 155, {255, 255, 255, 255});

            int startX = 50, startY = 250;
            for (int i = 0; i < 40; ++i) {
                int col = i % 10;
                int row = i / 10;
                int x = startX + col * 92;
                int y = startY + row * 92;

                if (i == kbIndex) {
                    fillRoundedRect(ren, x, y, 80, 80, 15, {255, 85, 0, 200}); // Glass Orange
                } else {
                    fillRoundedRect(ren, x, y, 80, 80, 15, {40, 40, 40, 120}); // Glass Black
                }

                std::string k = keyboard[i];
                SDL_Color tc = (i == kbIndex) ? SDL_Color{0, 0, 0, 255} : SDL_Color{255, 255, 255, 255};
                int textOffsetX = 20;
                if (k.length() > 1) textOffsetX = 5;
                textRenderer.drawText(k, x + textOffsetX, y + 25, tc);
            }
            textRenderer.drawText("D-PAD: Move | A: Type | B: Delete | START: Exit Menu", 50, 720, {200, 200, 200, 255});
        } 
        else if (state == STATE_LIST) {
            if (isOfflineMode) {
                textRenderer.drawText("OFFLINE LIBRARY (" + std::to_string(tracks.size()) + " tracks)", 50, 40, {255, 85, 0, 255});
            } else {
                textRenderer.drawText("RESULTS FOR: " + searchQuery, 50, 40, {255, 85, 0, 255});
            }
            
            int visibleCount = 11;
            int startIndex = selectedIndex - (visibleCount / 2);
            if (startIndex < 0) startIndex = 0;
            if (startIndex + visibleCount > (int)tracks.size()) {
                startIndex = (int)tracks.size() - visibleCount;
                if (startIndex < 0) startIndex = 0;
            }

            int startY = 120;
            for (int i = startIndex; i < startIndex + visibleCount && i < (int)tracks.size(); ++i) {
                int y = startY + (i - startIndex) * 50;
                
                if (i == selectedIndex) {
                    SDL_Rect selRect = {40, y - 5, 940, 45};
                    SDL_SetRenderDrawColor(ren, 50, 50, 50, 255);
                    SDL_RenderFillRect(ren, &selRect);
                }
                SDL_Color color = {200, 200, 200, 255};
                if (i == playingIndex) color = {255, 85, 0, 255};
                else if (i == selectedIndex) color = {255, 255, 255, 255};

                std::string displayText = (i == playingIndex ? "> " : "  ") + tracks[i].title;
                if (displayText.length() > 65) displayText = displayText.substr(0, 62) + "...";
                textRenderer.drawText(displayText, 50, y + 5, color);
            }

            if (tracks.size() > visibleCount) {
                int scrollbarHeight = 550 * visibleCount / tracks.size();
                int maxStartIndex = tracks.size() - visibleCount;
                int scrollbarY = 120 + (550 - scrollbarHeight) * startIndex / maxStartIndex;
                SDL_Rect scrollRect = {990, scrollbarY, 10, scrollbarHeight};
                SDL_SetRenderDrawColor(ren, 100, 100, 100, 255);
                SDL_RenderFillRect(ren, &scrollRect);
            }
            textRenderer.drawText("A: Play | B: Back to Search | SELECT: Screen Off | START: Exit", 50, 720, {150, 150, 150, 255});
        }
        else if (state == STATE_NOW_PLAYING) {
            const auto& t = tracks[playingIndex];
            
            int artSize = 460;
            int artX = (1024 - artSize) / 2;
            int artY = 50; 
            
            if (currentArtwork) {
                SDL_Rect dst = {artX, artY, artSize, artSize};
                SDL_RenderCopy(ren, currentArtwork, nullptr, &dst);
            } else {
                SDL_Rect dst = {artX, artY, artSize, artSize};
                SDL_SetRenderDrawColor(ren, 50, 50, 50, 255);
                SDL_RenderFillRect(ren, &dst);
                largeText.drawText("No Art", artX + 160, artY + 200, {150,150,150,255});
            }
            
            std::string title = t.title;
            if (title.length() > 40) title = title.substr(0, 37) + "...";
            largeText.drawText(title, artX - 100, artY + artSize + 20, {255, 255, 255, 255});
            textRenderer.drawText(t.artist, artX - 100, artY + artSize + 70, {180, 180, 180, 255});
            
            // Accurately calculated Progress Bar
            int duration = t.durationMs;
            if (duration <= 0) duration = 1;
            
            float progress = (float)currentPlaybackTime / duration;
            if (progress > 1.0f) progress = 1.0f;
            
            int barY = artY + artSize + 120;
            SDL_Rect barBg = {artX - 100, barY, 660, 6};
            SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);
            SDL_RenderFillRect(ren, &barBg);
            
            int barWidth = (int)(660 * progress);
            SDL_Rect barFg = {artX - 100, barY, barWidth, 6};
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderFillRect(ren, &barFg);
            SDL_Rect knob = {artX - 100 + barWidth - 8, barY - 5, 16, 16};
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderFillRect(ren, &knob);

            textRenderer.drawText(formatTime(currentPlaybackTime), artX - 100, barY + 20, {150, 150, 150, 255});
            textRenderer.drawText("-" + formatTime(duration - currentPlaybackTime), artX + 500, barY + 20, {150, 150, 150, 255});

            // Interactive Media Controls
            int btnY = barY + 40;
            SDL_Color colorPrev = (nowPlayingSelectedBtn == 0) ? SDL_Color{255,85,0,255} : SDL_Color{255,255,255,255};
            SDL_Color colorPlay = (nowPlayingSelectedBtn == 1) ? SDL_Color{255,85,0,255} : SDL_Color{255,255,255,255};
            SDL_Color colorNext = (nowPlayingSelectedBtn == 2) ? SDL_Color{255,85,0,255} : SDL_Color{255,255,255,255};

            largeText.drawText("|<", artX + 140, btnY, colorPrev);
            largeText.drawText(isPlaying ? "||" : ">", artX + 240, btnY, colorPlay);
            largeText.drawText(">|", artX + 340, btnY, colorNext);

            textRenderer.drawText("SELECT: Screen Off | B: List | Y: Save Offline", 50, 720, {150, 150, 150, 255});
            
            if (isDownloading && !isOfflineMode) {
                textRenderer.drawText(downloadStatusText, artX - 100, artY + artSize + 160, {255, 85, 0, 255});
            }
        }

        if (!isScreenOff) {
            smallText.drawText("github: nguientiendat", 810, 730, {150, 150, 150, 100});
        }
        
        // --- DRAW EXIT POPUP OVERLAY ---
        if (isConfirmingExit) {
            // Lớp nền đen mờ
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 200);
            SDL_Rect fullScreen = {0, 0, 1024, 768};
            SDL_RenderFillRect(ren, &fullScreen);
            
            // Hộp thoại popup bo góc mượt mà
            fillRoundedRect(ren, 262, 284, 500, 200, 20, {40, 35, 45, 240}); // Glass dark
            
            largeText.drawText("EXIT SOUNDBRICK?", 290, 320, {255, 255, 255, 255});
            textRenderer.drawText("A: Yes          B: No", 360, 410, {200, 200, 200, 255});
        }

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    if (bgTexture) SDL_DestroyTexture(bgTexture);
    if (currentArtwork) SDL_DestroyTexture(currentArtwork);
    stopMusic();
    if (controller) SDL_GameControllerClose(controller);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    std::cout << "Exiting SoundBrick." << std::endl;
    return 0;
}
