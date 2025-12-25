#include <sdbus-c++/sdbus-c++.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <iostream>
#include <unistd.h>
#include <limits.h>
#include <vector>
#include <string>
#include <fcntl.h>
#include <csignal>

std::string displayProgram = "";

int runApp(const std::string& path, const std::vector<std::string>& args) {
    if (path.size() == 0) {
        return -1;
    }
    pid_t pid = fork();
    if (pid == 0) {
        // child
        std::vector<char*> cargs;
        cargs.push_back(const_cast<char*>(path.c_str()));
        for (auto& s : args) {
            cargs.push_back(const_cast<char*>(s.c_str()));
        }
        cargs.push_back(nullptr);
        execvp(path.c_str(),cargs.data());
        perror("execvp failed");
        system("zenity --warning --text='exec fail'");
        _exit(1);
    } else if (pid > 0) {
        // parent
        return 0;
    } else {
        perror("fork failed!");
        system("zenity --warning --text='fork fail'");
        return -1;
    }
}

uint32_t cb_Notify(
    const std::string& app_name,
    uint32_t replaces_id,
    const std::string& app_icon,
    const std::string& summary,
    const std::string& body,
    const std::vector<std::string>& actions,
    const std::map<std::string,sdbus::Variant>& hints,
    int32_t timeout
) {
    if (displayProgram.size() == 0) {
        fprintf(stderr,"< ! > displayProgram is undefined. Should I be running?");
        return 0;
    }
    std::cout << "[" << app_name << "] " << summary << ": " << body << std::endl;
    runApp(displayProgram,{app_name,std::to_string(replaces_id),app_icon,summary,body,std::to_string(timeout)});
    return 1;
}

std::tuple<std::string,std::string,std::string,std::string> cb_ServerInfo() {
    return std::make_tuple(
        (std::string)"milo",
        (std::string)"cookiiq",
        (std::string)"1.0.0",
        (std::string)"1.2");
}

int main() {
    signal(SIGCHLD, SIG_IGN);
    const char* shm_name = "/my_shared_value";
    int shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    ftruncate(shmfd,sizeof(bool)*64);
    bool* notificationIndex = static_cast<bool*>(mmap(nullptr,sizeof(bool*)*64,PROT_READ|PROT_WRITE,MAP_SHARED,shmfd,0));
    for (int i = 0; i < 64; ++i) {
        notificationIndex[i] = true;
    }
    printf("Starting...\n");

    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe",path,PATH_MAX);
    if (count == -1) {
        fprintf(stderr,"< x > `readlink` failed. Cannot find display program.\n");
        return 1;
    }
    std::string fullPath(path,count);
    size_t finalSlash = fullPath.find_last_of("/");
    if (finalSlash == std::string::npos) {
        fprintf(stderr,"< x > Invalid path passed to string.find_last_of? Cannot find display program.\n");
        return 1;
    }
    displayProgram = fullPath.substr(0,finalSlash);
    displayProgram.append("/milo_display");
    printf("display program path: %s\n",displayProgram.c_str());

    sdbus::ServiceName serviceName{"org.freedesktop.Notifications"};

    auto conn = sdbus::createSessionBusConnection(serviceName);

    auto object = sdbus::createObject(*conn, sdbus::ObjectPath("/org/freedesktop/Notifications"));

    object->addVTable(
        {
            sdbus::registerMethod("Notify")
                .implementedAs(cb_Notify)
                .withInputParamNames({"app_name","replaces_id","app_icon","summary","body","actions","hints","timeout"})
                .withOutputParamNames({"notification_id"}),
            sdbus::registerMethod("GetServerInformation")
                .implementedAs(cb_ServerInfo)
                .withInputParamNames({})
                .withOutputParamNames({"name","vendor","version","spec_version"})
        }
    ).forInterface("org.freedesktop.Notifications");

    printf("Running!\n");
    conn->enterEventLoop();
    printf("Shutting down...\n");

    shm_unlink(shm_name);
    conn->releaseName(serviceName);
}

