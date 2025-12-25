#include <nlohmann/json.hpp>
#include <unicode/unistr.h>
#define Font X11_Font
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#undef Font
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <raylib.h>
#include <iostream>
#include <fcntl.h>
#include <fstream>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <any>
#include <stdlib.h>

std::string strip_illegal_chars(const std::string& input) {
    icu::UnicodeString u = icu::UnicodeString::fromUTF8(input);
    icu::UnicodeString out;

    for (int32_t i = 0; i < u.length();) {
        UChar32 character = u.char32At(i);
        i += U16_LENGTH(character);

        if (character == 0x200E || character == 0x200F || (character >= 0x202A && character <= 0x202E) || (character >= 0x2066 && character <= 0x2069)) {
            continue;
        }
        out.append(character);
    }
    std::string result;
    out.toUTF8String(result);
    return result;
}

std::optional<std::vector<std::string>> wrapText(
    const char* text,
    size_t maxWidth,
    float fontSize
) {
    size_t textLen = strlen(text);
    if (text[textLen] != '\0') {
        fprintf(stderr,"< ! > Text is illegal!");
        return std::nullopt;
    }
    std::string line = "";
    std::string word = "";
    std::vector<std::string> lines = {};
    for (size_t i=0;i<textLen+1;i++) {
        char character = text[i];
        if (character == ' ' or !character) {
            int size = MeasureText(line.c_str(),fontSize);
            printf("size: %d\n",size);
            if (size >= maxWidth) {
                line = line.substr(0,line.find_last_of(' '));
                lines.push_back(line);
                line = word;
            }
            word = "";
        } else {
            word.push_back(character);
        }
        line.push_back(character);
        if (character == '\n') {
            lines.push_back(line);
            line = "";
        }
    }
    printf("size: %d\n",MeasureText(line.c_str(),fontSize));
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
    std::string strSummary = strip_illegal_chars(summary);
    summary = strSummary.c_str();
    const char* body = argv[5];
    std::string strBody = strip_illegal_chars(body);
    body = strBody.c_str();
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
    auto wrappedSummary = wrapText(summary,280,20);
    if (!wrappedBody) {
        return 1;
    }
    if (!wrappedSummary) {
        return 1;
    }
    int wrappedSumHeight = ((wrappedSummary->size()-1)*20);
    SetWindowSize(300,75+((wrappedBody->size()-1)*10)+wrappedSumHeight);

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
        for (size_t i=0;i<wrappedSummary->size();i++) {
            DrawText(wrappedSummary.value()[i].c_str(),10,10+(i*20),20,BLACK);
        }
        // DrawText(summary,10,10+wrappedSumHeight,20,BLACK);
        DrawText(appname_display.c_str(),10,30+wrappedSumHeight,10,BLACK);
        for (size_t i=0;i<wrappedBody->size();i++) {
            DrawText(wrappedBody.value()[i].c_str(),10,55+(i*10)+wrappedSumHeight,10,BLACK);
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