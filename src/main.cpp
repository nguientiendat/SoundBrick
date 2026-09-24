#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <glob.h>
#include <SDL2/SDL.h>
#include "soundcloud/SoundCloudClient.h"
#include "ui/TextRenderer.h"
#include "network/HttpClient.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

// Hàm tải ảnh từ URL và chuyển thành Texture, đồng thời tính luôn màu chủ đạo
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
    
    // --- Tính màu nền nghệ thuật (Average Color) ---
    long long r = 0, g = 0, b = 0;
    int pixelCount = w * h;
    int step = 10; // Lấy mẫu 1/10 để chạy siêu tốc
    int samples = 0;
    for (int i = 0; i < pixelCount; i += step) {
        r += data[i*4 + 0];
        g += data[i*4 + 1];
        b += data[i*4 + 2];
        samples++;
    }
    // Giảm độ sáng đi 50-60% để chữ trắng nổi bật lên
    outBgColor.r = (r / samples) * 0.4f;
    outBgColor.g = (g / samples) * 0.4f;
    outBgColor.b = (b / samples) * 0.4f;
    outBgColor.a = 255;
    
    // --- Thuật toán cắt viền bo góc tròn (Rounded Corners) cực mượt ---
    int R = 30; // Bán kính bo góc (30 pixel)
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int cx = -1, cy = -1;
            // Xác định tâm của 4 đường tròn ở 4 góc
            if (x < R && y < R) { cx = R; cy = R; } // Góc trên trái
            else if (x > w - 1 - R && y < R) { cx = w - 1 - R; cy = R; } // Góc trên phải
            else if (x < R && y > h - 1 - R) { cx = R; cy = h - 1 - R; } // Góc dưới trái
            else if (x > w - 1 - R && y > h - 1 - R) { cx = w - 1 - R; cy = h - 1 - R; } // Góc dưới phải
            
            if (cx != -1 && cy != -1) {
                float dx = (float)(x - cx);
                float dy = (float)(y - cy);
                float dist = std::sqrt(dx*dx + dy*dy);
                
                // Nếu khoảng cách lớn hơn bán kính -> Nằm ngoài góc bo -> Xóa pixel (Alpha = 0)
                if (dist > R) {
                    data[(y * w + x) * 4 + 3] = 0;
                } else if (dist > R - 1.0f) {
                    // Viền của đường cong (khoảng cách lẻ tẻ) -> Làm mờ (Anti-aliasing) cho mịn
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
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND); // Kích hoạt hiệu ứng trong suốt (Alpha)
        SDL_FreeSurface(surf);
    }
    stbi_image_free(data);
    return tex;
}

// Hàm tiện ích dừng nhạc
void stopMusic() {
    system("killall -9 tplayerdemo >/dev/null 2>&1");
    system("killall -9 tail >/dev/null 2>&1");
}

enum AppState { STATE_LOGIN, STATE_SEARCH, STATE_LOADING, STATE_LIST, STATE_NOW_PLAYING };

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

    AppState state = sc.getClientId().empty() ? STATE_LOGIN : STATE_SEARCH;
    std::string searchQuery = "lofi"; 
    std::string loginInput = "";
    
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
    SDL_Color currentBgColor = {30, 25, 35, 255}; // Màu nền gradient
    
    // Biến quản lý thanh thời gian và playback
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

    // Tự động quét đường dẫn file điều khiển đèn nền (Backlight) của hệ thống
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

    while (isRunning) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                isRunning = false;
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) isRunning = false;
            } else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                int btn = e.cbutton.button;
                
                // Bật lại màn hình nếu đang tắt (bất kỳ nút nào)
                if (isScreenOff) {
                    isScreenOff = false;
                    system("echo 0 > /sys/class/graphics/fb0/blank 2>/dev/null");
                    if (!backlightPath.empty()) {
                        std::string cmd = "echo " + std::to_string(savedBrightness) + " > " + backlightPath + " 2>/dev/null";
                        system(cmd.c_str());
                    }
                    continue;
                }

                // Bấm SELECT (BACK) để tắt màn hình tiết kiệm pin
                if (btn == SDL_CONTROLLER_BUTTON_BACK) {
                    isScreenOff = true;
                    system("echo 1 > /sys/class/graphics/fb0/blank 2>/dev/null"); // Chuẩn chung Linux tắt màn hình
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
                    isRunning = false;
                }

                if (state == STATE_SEARCH || state == STATE_LOGIN) {
                    if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP) kbIndex -= 10;
                    if (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN) kbIndex += 10;
                    if (btn == SDL_CONTROLLER_BUTTON_DPAD_LEFT) kbIndex--;
                    if (btn == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) kbIndex++;
                    
                    if (kbIndex < 0) kbIndex += 40;
                    if (kbIndex >= 40) kbIndex -= 40;

                    if (btn == SDL_CONTROLLER_BUTTON_B) { // TrimUI phím A
                        std::string key = keyboard[kbIndex];
                        std::string& targetStr = (state == STATE_LOGIN) ? loginInput : searchQuery;
                        
                        if (key == "DEL") {
                            if (!targetStr.empty()) targetStr.pop_back();
                        } else if (key == "SPC") {
                            if (state == STATE_SEARCH) targetStr += " "; // Không cho khoảng trắng ở Client ID
                        } else if (key == "CLR") {
                            targetStr = "";
                        } else if (key == "GO") {
                            if (state == STATE_LOGIN) {
                                if (!loginInput.empty()) {
                                    sc.setClientId(loginInput);
                                    sc.saveConfig("data/settings.json");
                                    state = STATE_SEARCH;
                                }
                            } else {
                                if (!searchQuery.empty()) state = STATE_LOADING;
                            }
                        } else {
                            if (state == STATE_SEARCH || targetStr.length() < 32) {
                                targetStr += key;
                            }
                        }
                    } else if (btn == SDL_CONTROLLER_BUTTON_A) { // TrimUI phím B
                        std::string& targetStr = (state == STATE_LOGIN) ? loginInput : searchQuery;
                        if (!targetStr.empty()) targetStr.pop_back();
                    } else if (btn == SDL_CONTROLLER_BUTTON_X) { // Nút Y trên TrimUI
                        if (state == STATE_LOGIN) {
                            std::cout << "Auto fetching client ID..." << std::endl;
                            loginInput = sc.autoFetchClientId();
                            if (!loginInput.empty()) {
                                sc.setClientId(loginInput);
                                sc.saveConfig("data/settings.json");
                                state = STATE_SEARCH;
                            }
                        }
                    }
                } 
                else if (state == STATE_LIST) {
                    if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP) selectedIndex--;
                    if (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN) selectedIndex++;
                    
                    if (btn == SDL_CONTROLLER_BUTTON_B) { // Phím A (Right)
                        if (!tracks.empty() && selectedIndex >= 0 && selectedIndex < (int)tracks.size()) {
                            if (playingIndex != selectedIndex) {
                                playingIndex = selectedIndex;
                                needLoadArtwork = true;
                            }
                            state = STATE_NOW_PLAYING;
                        }
                    } else if (btn == SDL_CONTROLLER_BUTTON_A) { // Phím B (Down)
                        state = STATE_SEARCH;
                    }
                }
                else if (state == STATE_NOW_PLAYING) {
                    if (btn == SDL_CONTROLLER_BUTTON_A) { // Phím B (Down) quay lại List
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
                    else if (btn == SDL_CONTROLLER_BUTTON_B) { // Phím A (Right) thao tác nút bấm
                        if (nowPlayingSelectedBtn == 0) { // Nút Previous
                            if (playingIndex > 0) {
                                playingIndex--;
                                needLoadArtwork = true;
                            }
                        } else if (nowPlayingSelectedBtn == 2) { // Nút Next
                            if (playingIndex < (int)tracks.size() - 1) {
                                playingIndex++;
                                needLoadArtwork = true;
                            }
                        } else if (nowPlayingSelectedBtn == 1) { // Toggle Play/Pause
                            isPlaying = !isPlaying;
                            if (isPlaying) {
                                // Resume bằng tín hiệu SIGCONT
                                system("killall -CONT tplayerdemo >/dev/null 2>&1");
                                playStartTime = SDL_GetTicks() - currentPlaybackTime;
                            } else {
                                // Pause bằng tín hiệu SIGSTOP
                                system("killall -STOP tplayerdemo >/dev/null 2>&1");
                                currentPlaybackTime = SDL_GetTicks() - playStartTime;
                            }
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
            std::string streamUrl = sc.resolveStreamUrl(tracks[playingIndex].permalink);
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
            
            // Reset thời gian
            currentPlaybackTime = 0;
            playStartTime = SDL_GetTicks();
            isPlaying = true;
            nowPlayingSelectedBtn = 1;
            continue;
        }
        
        // Mô phỏng thanh thời gian tự chạy
        if (state == STATE_NOW_PLAYING && isPlaying) {
            currentPlaybackTime = SDL_GetTicks() - playStartTime;
            if (currentPlaybackTime > (Uint32)tracks[playingIndex].durationMs && tracks[playingIndex].durationMs > 0) {
                // Tự động chuyển bài khi hết nhạc (Loop lại từ đầu nếu hết danh sách)
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
            SDL_Delay(100); // Sleep dài hơn để CPU được nghỉ ngơi tối đa
            continue;
        }

        if (state == STATE_NOW_PLAYING) {
            // Vẽ Gradient cực kỳ nghệ thuật từ màu chủ đạo xuống màu xám đen
            for (int i = 0; i < 768; i++) {
                float t = (float)i / 768.0f;
                // Hàm t^2 để gradient cong nhẹ, giúp viền dưới đen nhánh hơn
                float factor = t * t; 
                Uint8 r = currentBgColor.r * (1.0f - factor) + 18 * factor;
                Uint8 g = currentBgColor.g * (1.0f - factor) + 18 * factor;
                Uint8 b = currentBgColor.b * (1.0f - factor) + 18 * factor;
                SDL_SetRenderDrawColor(ren, r, g, b, 255);
                SDL_RenderDrawLine(ren, 0, i, 1024, i);
            }
        } 
        else {
            SDL_SetRenderDrawColor(ren, 18, 18, 18, 255);
            SDL_RenderClear(ren);
        }

        if (state == STATE_SEARCH || state == STATE_LOGIN) {
            if (state == STATE_LOGIN) {
                largeText.drawText("SOUNDCLOUD LOGIN", 50, 40, {255, 85, 0, 255});
                textRenderer.drawText("Enter your 32-character Client ID:", 50, 100, {200, 200, 200, 255});
            } else {
                largeText.drawText("SOUNDBRICK SEARCH", 50, 50, {255, 85, 0, 255});
            }
            
            SDL_Rect inputRect = {50, 150, 924, 60};
            SDL_SetRenderDrawColor(ren, 40, 40, 40, 255);
            SDL_RenderFillRect(ren, &inputRect);
            
            std::string displayStr = (state == STATE_LOGIN ? loginInput : searchQuery) + "_";
            largeText.drawText(displayStr, 70, 160, {255, 255, 255, 255});

            int startX = 50, startY = 250;
            for (int i = 0; i < 40; ++i) {
                int col = i % 10;
                int row = i / 10;
                int x = startX + col * 92;
                int y = startY + row * 92;

                if (i == kbIndex) {
                    SDL_Rect keyRect = {x, y, 80, 80};
                    SDL_SetRenderDrawColor(ren, 255, 85, 0, 255);
                    SDL_RenderFillRect(ren, &keyRect);
                } else {
                    SDL_Rect keyRect = {x, y, 80, 80};
                    SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
                    SDL_RenderFillRect(ren, &keyRect);
                }

                std::string k = keyboard[i];
                SDL_Color tc = (i == kbIndex) ? SDL_Color{0, 0, 0, 255} : SDL_Color{255, 255, 255, 255};
                int textOffsetX = 20;
                if (k.length() > 1) textOffsetX = 5;
                textRenderer.drawText(k, x + textOffsetX, y + 25, tc);
            }
            if (state == STATE_LOGIN) {
                textRenderer.drawText("Y: Auto Fetch from Internet | A: Type | B: Delete", 50, 720, {150, 150, 150, 255});
            } else {
                textRenderer.drawText("Y: Config ID | D-PAD: Move | A: Type | B: Delete | SELECT: Screen Off", 50, 720, {150, 150, 150, 255});
            }
        } 
        else if (state == STATE_LIST) {
            textRenderer.drawText("RESULTS FOR: " + searchQuery, 50, 40, {255, 85, 0, 255});
            
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
            
            // Progress Bar được tính toán thực tế
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

            // Thanh Media Controls tương tác được
            int btnY = barY + 40;
            SDL_Color colorPrev = (nowPlayingSelectedBtn == 0) ? SDL_Color{255,85,0,255} : SDL_Color{255,255,255,255};
            SDL_Color colorPlay = (nowPlayingSelectedBtn == 1) ? SDL_Color{255,85,0,255} : SDL_Color{255,255,255,255};
            SDL_Color colorNext = (nowPlayingSelectedBtn == 2) ? SDL_Color{255,85,0,255} : SDL_Color{255,255,255,255};

            largeText.drawText("|<", artX + 140, btnY, colorPrev);
            largeText.drawText(isPlaying ? "||" : ">", artX + 240, btnY, colorPlay);
            largeText.drawText(">|", artX + 340, btnY, colorNext);

            textRenderer.drawText("SELECT: Screen Off | B: Back to List", 50, 740, {150, 150, 150, 255});
        }

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    if (currentArtwork) SDL_DestroyTexture(currentArtwork);
    stopMusic();
    if (controller) SDL_GameControllerClose(controller);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    std::cout << "Exiting SoundBrick." << std::endl;
    return 0;
}
