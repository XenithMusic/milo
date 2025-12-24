#include <sdbus-c++/sdbus-c++.h>
#include <iostream>
#include <vector>

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
    std::cout << "[" << app_name << "] " << summary << ": " << body << std::endl;
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

    conn->enterEventLoop();

    conn->releaseName(serviceName);
}

