
#include <libsnore/snore.h>
#include <libsnore/notification/notification.h>
#include <libsnore/log.h>
#include <libsnore/version.h>
#include <libsnore/utils.h>

#include <QApplication>
#include <QCommandLineParser>

#include <iostream>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <winuser.h>
#endif

using namespace Snore;
using namespace std;


void bringToFront(QString pid)
{
    snoreDebug(SNORE_DEBUG) << pid;
#ifdef Q_OS_WIN
    auto findWindowForPid = [](ulong pid) {
        // based on http://stackoverflow.com/a/21767578
        pair<ulong, HWND> data = make_pair(pid, (HWND)0);
        ::EnumWindows((WNDENUMPROC)static_cast<BOOL(*)(HWND,LPARAM)>([](HWND handle, LPARAM lParam) -> BOOL {
            auto isMainWindow = [](HWND handle)
            {
                return ::GetWindow(handle, GW_OWNER) == (HWND)0 && IsWindowVisible(handle);
            };
            pair<ulong, HWND> &data = *(pair<ulong, HWND> *)lParam;
            ulong process_id = 0;
            ::GetWindowThreadProcessId(handle, &process_id);
            if (data.first != process_id || !isMainWindow(handle))
            {
                return TRUE;
            }
            data.second = handle;
            return FALSE;
        }), (LPARAM)&data);
        return data.second;
    };

    HWND wid = findWindowForPid(pid.toInt());
    if (wid) {
        Utils::bringWindowToFront((WId)wid, true);
    }
#endif
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("snore-send");
    app.setOrganizationName("Snorenotify");
    app.setApplicationVersion(Snore::Version::version());

    QCommandLineParser parser;
    parser.setApplicationDescription("A command line interface for Snorenotify.");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption title(QStringList() << "t" << "title", "Set the notification title.", "title");
    parser.addOption(title);

    QCommandLineOption message(QStringList() << "m" << "message", "Set the notification body.", "message");
    parser.addOption(message);

    QCommandLineOption applicationName(QStringList() << "a" << "application", "Set the notification applicattion.", "application", app.applicationName());
    parser.addOption(applicationName);

    QCommandLineOption alertName(QStringList() << "c" << "alert", "Set the notification alert class.", "alert", "Default");
    parser.addOption(alertName);

    QCommandLineOption iconPath(QStringList() << "i" << "icon", "Set the notification icon.", "icon", ":/root/snore.png");
    parser.addOption(iconPath);

    QCommandLineOption priority(QStringList() << "p" << "priority", "Set the notification's' priority.", "[-2, 2]", "0");
    parser.addOption(priority);

    QCommandLineOption markup(QStringList() << "markup", "Enable markup support.", "[0,1]", "0");
    parser.addOption(markup);

    QCommandLineOption silent(QStringList() << "silent", "Don't print to stdout.");
    parser.addOption(silent);

    QCommandLineOption _bringProcessToFront(QStringList() << "bring-process-to-front", "Bring process with pid to front if notification is clicked.", "pid");
    parser.addOption(_bringProcessToFront);

    QCommandLineOption _bringWindowToFront(QStringList() << "bring-window-to-front", "Bring window with wid to front if notification is clicked.", "wid");
    parser.addOption(_bringWindowToFront);

    parser.process(app);
    snoreDebug(SNORE_DEBUG) << app.arguments();
    if (parser.isSet(title) && parser.isSet(message)) {
        SnoreCore &core = SnoreCore::instance();

        core.loadPlugins(SnorePlugin::BACKEND | SnorePlugin::SECONDARY_BACKEND);

        Icon icon(parser.value(iconPath));
        Application application(parser.value(applicationName), icon);
        Alert alert(parser.value(alertName), icon);
        application.addAlert(alert);

        if(parser.value(markup).toInt() == 1)
        {
            application.hints().setValue("use-markup", QVariant::fromValue(true));
        }

        core.registerApplication(application);

        int prio = parser.value(priority).toInt();
        if(prio < -2 || prio > 2){
            parser.showHelp(-1);
        }
        Notification n(application, alert, parser.value(title), parser.value(message), icon, Notification::defaultTimeout(), static_cast<Notification::Prioritys>(prio));
        if (parser.isSet(_bringProcessToFront) || parser.isSet(_bringWindowToFront)) {
            n.addAction(Action(1, "Bring to Front"));
        }
        int returnCode = -1;

        app.connect(&core, &SnoreCore::notificationClosed, [&](Notification noti) {
            if (!parser.isSet(silent)) {
                QString reason;
                QDebug(&reason) << noti.closeReason();
                cout << qPrintable(reason) << endl;
            }
            if (noti.closeReason() == Notification::CLOSED) {
                if (parser.isSet(_bringProcessToFront)) {
                    bringToFront(parser.value(_bringProcessToFront));
                } else if (parser.isSet(_bringWindowToFront)) {
                    Utils::bringWindowToFront((WId)parser.value(_bringWindowToFront).toULongLong(),true);
                }
            }
            returnCode = noti.closeReason();
        });
        app.connect(&core, &SnoreCore::notificationClosed, &app, &QApplication::quit);
        app.processEvents();
        core.broadcastNotification(n);

        app.exec();
        return returnCode;
    }
    parser.showHelp(1);
}
