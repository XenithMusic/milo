#include <nlohmann/json.hpp>
#define Font X11_Font
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#undef Font
#include <sys/mman.h>
#include <sys/stat.h>
#include <raylib.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <any>
#include <fcntl.h>
#include <unistd.h>

std::string popLastWord(std::string& line) {
    if (line.empty()) {
        return {};
    }

    size_t end = line.find_last_not_of(' ');
    if (end == std::string::npos) {
        line.clear();
        return {};
    }

    size_t start = line.find_last_of(' ', end);
    size_t wordStart = (start == std::string::npos) ? 0 : start + 1;

    std::string word = line.substr(wordStart, end - wordStart + 1);
    line.erase(wordStart);

    return word;
}

std::vector<std::string> wrapText(
    const char* text,
    size_t maxWidth,
    float fontSize
) {
    size_t textLen = strlen(text);
    std::string line = "";
    std::vector<std::string> lines = {};
    for (size_t i=0;i<textLen;i++) {
        char character = text[i];
        line.push_back(character);
        if (character == ' ') {
            int size = MeasureText(line.c_str(),fontSize);
            if (size > maxWidth) {
                std::string lastWord = popLastWord(line);
                lines.push_back(line);
                line = lastWord;
                line.push_back(character);
            }
            continue;
        }
        if (character == '\n') {
            lines.push_back(line);
            line = "";
        }
    }
    lines.push_back(line);
    return lines;
}

int main(int argc,char* argv[]) {
    if (argc <= 6) {
        fprintf(stderr,"< x > Not enough arguments passed to milo_display! expected 5 args, got %d\n", argc-1);
        return 1;
    }

    printf("Loading configuration...\n");
    const char* chome = std::getenv("XDG_CONFIG_HOME");
    std::string home;
    if (!chome) {
        home = std::string(std::getenv("HOME")) + "/.config/";
    } else {
        home = std::string(chome);
    }
    home.append("milo/config.json");
    std::ifstream file(home);
    if (!file.is_open()) {
        fprintf(stderr,"< x > Failed to open configuration at '%s'.\n",home.c_str());
        return 1;
    }

    nlohmann::json config;
    file >> config;
    file.close();

    const char* app_name = argv[1];
    const char* app_icon = argv[3];
    const char* summary = argv[4];
    const char* body = argv[5];
    int timeout = std::min((int)config["maxTimeout"],std::stoi(argv[6]));
    if (timeout == 0) {
        timeout = 5000;
    }
    if (timeout == -1) {
        timeout = (int)config["maxTimeout"];
    }
    printf("Notification will remain visible for %d milliseconds.\n",timeout);
    // const std::vector<std::string>& actions = argv[6];

    SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNFOCUSED);
    InitWindow(300,75,"milo notification");
    int currentMonitor = GetCurrentMonitor();
    Vector2 pos = GetMonitorPosition(currentMonitor);
    int monitorWidth = GetMonitorWidth(currentMonitor);
    SetTargetFPS(15);

    auto wrappedBody = wrapText(body,280,10);
    auto wrappedSummary = wrapText(summary,270,20);
    int wrappedSumHeight = ((wrappedSummary.size()-1)*20);
    SetWindowSize(300,75+((wrappedBody.size()-1)*10)+wrappedSumHeight);

    const char* shm_name = "/my_shared_value";

    int shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    ftruncate(shmfd,sizeof(bool)*64);

    bool* notificationIndex = static_cast<bool*>(mmap(nullptr,sizeof(bool)*64,PROT_READ|PROT_WRITE,MAP_SHARED,shmfd,0));
    int myNotif = 0;
    for (int i = 0; i < 64; ++i) {
        if (notificationIndex[i] == true) {
            myNotif = i;
            notificationIndex[i] = false;
            break;
        }
    }

    printf("This notification is at position %d\n",myNotif);
    
    Display* d = XOpenDisplay(nullptr);
    if (!d) {
        fprintf(stderr,"Cannot open display\n");
        return 1;
    }
    Window root = DefaultRootWindow(d);
    Window focused;
    int revert;
    XGetInputFocus(d, &focused, &revert);
    Window root_return,child_return;
    int root_x,root_y;
    int win_x,win_y;
    uint mask_return;
    if (!XQueryPointer(
        d,
        root,
        &root_return,
        &child_return,
        &root_x,
        &root_y,
        &win_x,
        &win_y,
        &mask_return
    )) {
        fprintf(stderr,"Pointer is not on the same screen\n");
        return 1;
    }

    ClearWindowState(FLAG_WINDOW_HIDDEN);
    SetWindowPosition(pos.x+monitorWidth-GetScreenWidth()-15,pos.y+15+(myNotif*(GetScreenHeight()+10)));
    if (focused != None) {
        XSetInputFocus(d, focused, RevertToPointerRoot, CurrentTime);

        XWarpPointer(
            d,
            None,
            root,
            0,0,0,0,
            root_x,
            root_y
        );

        XFlush(d);
    }

    auto start = (std::chrono::system_clock::now());
    auto duration = std::chrono::milliseconds(timeout);
    auto now = std::chrono::system_clock::now();
    std::chrono::milliseconds elapsed;

    while (not WindowShouldClose()) {
        std::string appname_display = "from ";
        appname_display.append(app_name);
        BeginDrawing();
        ClearBackground({255,255,255,255});
        for (size_t i=0;i<wrappedSummary.size();i++) {
            DrawText(wrappedSummary[i].c_str(),10,10+(i*20),20,BLACK);
        }
        // DrawText(summary,10,10+wrappedSumHeight,20,BLACK);
        DrawText(appname_display.c_str(),10,30+wrappedSumHeight,10,BLACK);
        for (size_t i=0;i<wrappedBody.size();i++) {
            DrawText(wrappedBody[i].c_str(),10,55+(i*10)+wrappedSumHeight,10,BLACK);
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            break;
        }
        if (timeout != -1) {
            now = std::chrono::system_clock::now();
            elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now-start);
            DrawRectangle(0,0,(int)(((elapsed.count()/((float)timeout)))*GetScreenWidth()),5,{200,200,200,255});
            if (elapsed >= duration) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                break;
            }
        }
        EndDrawing();
    }
    printf("We exited.");
    notificationIndex[myNotif] = true;
    printf("One.\n");
    munmap(notificationIndex,sizeof(bool)*64);
    printf("Two.\n");
    close(shmfd);
    printf("Three.\n");
    CloseWindow();
    printf("Four.\n");
    XCloseDisplay(d);
    printf("Five.\n");
    return 0;
    printf("Six; unreachable.\n");
}