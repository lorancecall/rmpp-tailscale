// rmpp-tailscale: Tailscale settings page for reMarkable Paper Pro (xochitl), loaded with LD_PRELOAD.
// Injects a "Tailscale" row into Settings with on/off, login (QR code of the login URL), status and peers.
// Built without moc: QML talks to C++ through a QQmlPropertyMap ("ts").
// tailscaled runs as rmpp-tailscaled.service (userspace networking); this file only drives the tailscale CLI.
#include <algorithm>
#include <cstring>
#include <functional>
#include <QtCore/QBuffer>
#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QProcess>
#include <QtCore/QResource>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QWindow>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlPropertyMap>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

#include "qrcodegen.hpp"

namespace {

const QString SHARE = QStringLiteral("/home/root/.vellum/share/rmpp-tailscale");
const QString CLI = SHARE + QStringLiteral("/tailscale");
const QString SOCKET = QStringLiteral("/run/rmpp-tailscale/tailscaled.sock");
const QString CONF_DIR = QStringLiteral("/home/root/.config/rmpp-tailscale");
const QString ENABLED_FLAG = CONF_DIR + QStringLiteral("/enabled");
const QString SERVICE = QStringLiteral("rmpp-tailscaled.service");
const QString DEFAULT_HOSTNAME = QStringLiteral("rmpp");

// Login URL -> QR code PNG as a data: URL for a QML Image.
QString qrDataUrl(const QString &text) {
    if (text.isEmpty()) return {};
    using qrcodegen::QrCode;
    const QrCode qr = QrCode::encodeText(text.toUtf8().constData(), QrCode::Ecc::MEDIUM);
    const int border = 4, scale = 10, n = qr.getSize();
    QImage img((n + 2 * border) * scale, (n + 2 * border) * scale, QImage::Format_Grayscale8);
    img.fill(255);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            if (qr.getModule(x, y))
                for (int dy = 0; dy < scale; ++dy) {
                    uchar *line = img.scanLine((y + border) * scale + dy);
                    memset(line + (x + border) * scale, 0, scale);
                }
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64());
}

class Tailscale {
public:
    QQmlPropertyMap *map = nullptr;
    QObject *owner = nullptr;
    QTimer *poll = nullptr;
    QProcess *statusProc = nullptr;
    QProcess *action = nullptr;  // login / up / down / logout: one at a time
    int watchers = 0;
    QString lastAuthUrl;
    qint64 lastCheck = 0;  // ms since epoch of the last upstream version check

    // "1.104.5" < "1.106.0"
    static bool versionLess(const QString &a, const QString &b) {
        const QStringList x = a.split('.'), y = b.split('.');
        for (int i = 0; i < qMax(x.size(), y.size()); ++i) {
            const int p = i < x.size() ? x[i].toInt() : 0, q = i < y.size() ? y[i].toInt() : 0;
            if (p != q) return p < q;
        }
        return false;
    }

    // Ask pkgs.tailscale.com (through the CLI) for the newest stable version.
    void checkUpdate(bool force) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (map->value("checking").toBool() || !QFileInfo::exists(CLI)) return;
        if (!force && lastCheck && now - lastCheck < 3600 * 1000) return;
        lastCheck = now;
        set("checking", true);
        if (force) set("updateStatus", "Checking for updates…");
        auto *p = new QProcess(owner);
        QObject::connect(p, &QProcess::finished, owner, [this, p](int code, QProcess::ExitStatus) {
            const QJsonObject v = QJsonDocument::fromJson(p->readAllStandardOutput()).object();
            p->deleteLater();
            set("checking", false);
            const QString cur = v.value("short").toString(), up = v.value("upstream").toString();
            if (code != 0 || up.isEmpty()) {
                set("updateStatus", "Could not check for updates (no internet?)");
                return;
            }
            set("version", cur);
            set("latest", up);
            set("updateAvailable", versionLess(cur, up));
            set("updateStatus", QString());
        });
        p->start(CLI, cli({"version", "--upstream", "--json"}));
    }

    // `tailscale update` downloads the signed tarball, checks it and replaces tailscale + tailscaled
    // in place; then restart the daemon so the new version runs. A package upgrade puts back the
    // packaged version.
    void update() {
        const QString target = map->value("latest").toString();
        runAction("Updating Tailscale to " + target + "…", {"update", "--yes"}, [this](int code, const QString &out) {
            if (code != 0) {
                set("status", code > 0 ? lastLine(out) : QString());
                return;
            }
            set("status", "Restarting Tailscale…");
            systemctl("try-restart", [this] {
                whenDaemonReady([this] { set("status", QString()); checkUpdate(true); refresh(); });
            });
        });
    }

    explicit Tailscale(QObject *parent) : owner(parent) {
        map = new QQmlPropertyMap(parent);
        for (const char *k : {"state", "authUrl", "qr", "hostname", "dnsName", "ip4", "ip6", "tailnet", "user",
                              "peers", "status", "version", "latest", "updateStatus", "command"})
            map->insert(QString::fromLatin1(k), QString());
        for (const char *k : {"installed", "enabled", "daemon", "busy", "updateAvailable", "checking"})
            map->insert(QString::fromLatin1(k), false);
        QObject::connect(map, &QQmlPropertyMap::valueChanged, parent, [this](const QString &key, const QVariant &v) {
            if (key == QLatin1String("command")) command(v.toString().section('#', 0, 0));
        });
        poll = new QTimer(parent);
        QObject::connect(poll, &QTimer::timeout, parent, [this] { refresh(); });
    }

    void set(const char *k, const QVariant &v) {
        if (map->value(QString::fromLatin1(k)) != v) map->insert(QString::fromLatin1(k), v);
    }

    QStringList cli(const QStringList &args) const { return QStringList{QStringLiteral("--socket=") + SOCKET} + args; }

    // Run a long-ish CLI action (login waits until the user finishes in the browser).
    void runAction(const QString &label, const QStringList &args, std::function<void(int, const QString &)> done = {}) {
        if (action) { action->kill(); action->deleteLater(); action = nullptr; }
        auto *p = new QProcess(owner);
        action = p;
        p->setProcessChannelMode(QProcess::MergedChannels);
        set("busy", true);
        set("status", label);
        // done() gets -1 when the process was killed (cancel, or replaced by another action).
        QObject::connect(p, &QProcess::finished, owner, [this, p, done](int code, QProcess::ExitStatus st) {
            const QString out = QString::fromUtf8(p->readAll()).trimmed();
            if (action == p) { action = nullptr; set("busy", false); }
            p->deleteLater();
            if (done) done(st == QProcess::CrashExit ? -1 : code, out);
            refresh();
        });
        p->start(CLI, cli(args));
    }

    static QString lastLine(const QString &out) {
        const QStringList lines = out.split('\n', Qt::SkipEmptyParts);
        return lines.isEmpty() ? QString() : lines.last().trimmed();
    }

    void systemctl(const QString &verb, std::function<void()> then = {}) {
        auto *p = new QProcess(owner);
        QObject::connect(p, &QProcess::finished, owner, [p, then](int, QProcess::ExitStatus) {
            p->deleteLater();
            if (then) then();
        });
        p->start(QStringLiteral("systemctl"), {verb, SERVICE});
    }

    // tailscaled needs a moment after start before the socket answers.
    void whenDaemonReady(std::function<void()> then, int tries = 20) {
        if (QFileInfo::exists(SOCKET) || tries <= 0) { then(); return; }
        QTimer::singleShot(500, owner, [this, then, tries] { whenDaemonReady(then, tries - 1); });
    }

    QStringList upFlags() const {
        return {QStringLiteral("--hostname=") + DEFAULT_HOSTNAME, QStringLiteral("--accept-dns=false")};
    }

    void command(const QString &c) {
        const QString verb = c.section(':', 0, 0);
        if (verb == "active") {
            watchers = qMax(0, watchers + (c.section(':', 1) == "1" ? 1 : -1));
            if (watchers > 0) { refresh(); checkUpdate(false); if (!poll->isActive()) poll->start(2000); }
            else poll->stop();
        } else if (verb == "power") {
            if (c.section(':', 1) == "1") turnOn();
            else turnOff();
        } else if (verb == "login") {
            // Prints the login URL, then waits until the browser login finishes. The URL also shows up
            // in `status --json` (AuthURL), which is where the page reads it from.
            lastAuthUrl.clear();
            runAction("Getting a login link…", QStringList{"login"} + upFlags(), [this](int code, const QString &out) {
                set("status", code > 0 ? lastLine(out) : QString());
            });
        } else if (verb == "cancel") {
            if (action) { action->kill(); }
            set("status", QString());
        } else if (verb == "checkUpdate") {
            checkUpdate(true);
        } else if (verb == "update") {
            update();
        } else if (verb == "logout") {
            runAction("Logging out…", {"logout"}, [this](int code, const QString &out) {
                set("status", code > 0 ? lastLine(out) : QString());
            });
        }
    }

    void turnOn() {
        QDir().mkpath(CONF_DIR);
        QFile f(ENABLED_FLAG);
        if (!f.open(QIODevice::WriteOnly)) qWarning("[rmpp-tailscale] cannot write %s", qPrintable(ENABLED_FLAG));
        f.close();
        set("enabled", true);
        set("busy", true);
        set("status", "Starting Tailscale…");
        systemctl("start", [this] {
            whenDaemonReady([this] {
                // `up` returns at once when already logged in. When a login is needed it waits until the
                // login in the browser is done; meanwhile the page shows the URL from `status --json`.
                runAction("Connecting…", QStringList{"up"} + upFlags(), [this](int code, const QString &out) {
                    set("status", code > 0 ? lastLine(out) : QString());
                });
            });
        });
    }

    void turnOff() {
        QFile::remove(ENABLED_FLAG);
        set("enabled", false);
        if (action) action->kill();
        runAction("Turning off…", {"down"}, [this](int, const QString &) {
            systemctl("stop", [this] { set("status", QString()); refresh(); });
        });
    }

    void clearStatus() {
        for (const char *k : {"state", "authUrl", "qr", "dnsName", "ip4", "ip6", "tailnet", "user", "peers"})
            set(k, QString());
        lastAuthUrl.clear();
    }

    void refresh() {
        set("installed", QFileInfo::exists(CLI));
        set("enabled", QFileInfo::exists(ENABLED_FLAG));
        if (!QFileInfo::exists(SOCKET)) {
            set("daemon", false);
            clearStatus();
            return;
        }
        if (statusProc) return;  // previous one still running
        statusProc = new QProcess(owner);
        QObject::connect(statusProc, &QProcess::finished, owner, [this](int code, QProcess::ExitStatus) {
            const QByteArray out = statusProc->readAllStandardOutput();
            statusProc->deleteLater();
            statusProc = nullptr;
            if (code != 0) { set("daemon", false); clearStatus(); return; }
            set("daemon", true);
            parse(QJsonDocument::fromJson(out).object());
        });
        statusProc->start(CLI, cli({"status", "--json"}));
    }

    void parse(const QJsonObject &s) {
        const QString state = s.value("BackendState").toString();
        set("state", state);
        set("version", s.value("Version").toString().section('-', 0, 0));
        const QString url = state == "NeedsLogin" ? s.value("AuthURL").toString() : QString();
        if (url != lastAuthUrl) {
            lastAuthUrl = url;
            set("authUrl", url);
            set("qr", qrDataUrl(url));
        }
        const QJsonObject self = s.value("Self").toObject();
        set("hostname", self.value("HostName").toString());
        QString dns = self.value("DNSName").toString();
        if (dns.endsWith('.')) dns.chop(1);
        set("dnsName", dns);
        QString ip4, ip6;
        for (const auto &v : s.value("TailscaleIPs").toArray()) {
            const QString ip = v.toString();
            if (ip.contains(':')) { if (ip6.isEmpty()) ip6 = ip; }
            else if (ip4.isEmpty()) ip4 = ip;
        }
        set("ip4", ip4);
        set("ip6", ip6);
        set("tailnet", s.value("CurrentTailnet").toObject().value("Name").toString());
        const QString uid = QString::number(self.value("UserID").toVariant().toLongLong());
        set("user", s.value("User").toObject().value(uid).toObject().value("LoginName").toString());

        QJsonArray peers;
        const QJsonObject pm = s.value("Peer").toObject();
        for (auto it = pm.begin(); it != pm.end(); ++it) {
            const QJsonObject p = it.value().toObject();
            QString name = p.value("DNSName").toString().section('.', 0, 0);
            if (name.isEmpty()) name = p.value("HostName").toString();
            QJsonObject o;
            o["name"] = name;
            o["os"] = p.value("OS").toString();
            o["online"] = p.value("Online").toBool();
            const QJsonArray ips = p.value("TailscaleIPs").toArray();
            o["ip"] = ips.isEmpty() ? QString() : ips.first().toString();
            peers.append(o);
        }
        QList<QJsonValue> sorted(peers.begin(), peers.end());
        std::sort(sorted.begin(), sorted.end(), [](const QJsonValue &a, const QJsonValue &b) {
            const bool ao = a.toObject().value("online").toBool(), bo = b.toObject().value("online").toBool();
            if (ao != bo) return ao;
            return a.toObject().value("name").toString().toLower() < b.toObject().value("name").toString().toLower();
        });
        QJsonArray out;
        for (const auto &v : sorted) out.append(v);
        set("peers", QString::fromUtf8(QJsonDocument(out).toJson(QJsonDocument::Compact)));
    }
};

Tailscale *g_ts = nullptr;

// Depth-first search of the visual item tree (QML items are not always QObject children).
QQuickItem *findItem(QQuickItem *root, const QString &name, int depth = 0) {
    if (!root || depth > 200) return nullptr;
    if (root->objectName() == name) return root;
    const auto kids = root->childItems();
    for (QQuickItem *k : kids)
        if (QQuickItem *f = findItem(k, name, depth + 1)) return f;
    return nullptr;
}

QQuickItem *findByName(QObject *root, const char *name) {
    if (!root) return nullptr;
    const QString n = QString::fromLatin1(name);
    if (auto *item = qobject_cast<QQuickItem *>(root))
        if (QQuickItem *f = findItem(item, n)) return f;
    if (auto *it = root->findChild<QQuickItem *>(n)) return it;
    return nullptr;
}

QQuickItem *createFrom(QQmlEngine *eng, QQmlContext *ctx, const char *url, QQuickItem *parent) {
    QQmlComponent comp(eng, QUrl(QString::fromLatin1(url)));
    QObject *o = comp.create(ctx);
    if (!o) {
        for (const auto &e : comp.errors()) qWarning("[rmpp-tailscale] %s", qPrintable(e.toString()));
        return nullptr;
    }
    auto *item = qobject_cast<QQuickItem *>(o);
    if (!item) { delete o; return nullptr; }
    item->setParentItem(parent);
    item->setParent(parent);
    return item;
}

// ---- Settings rail order shared by the rmpp add-ons ----
// Every rmpp row carries an int property "rmppOrder": rmpp-bt 100, rmpp-tailscale 200, rmpp-lang 300 (Languages),
// rmpp-hwr 400 (Handwriting). Each add-on places only its own row, right after the rmpp row with the highest order
// below its own, else after Wi-Fi, so the order is the same whichever add-on injects first (no later re-sorting,
// no rows jumping after the page is shown). Rows without the property are recognised by class / objectName.
int railOrder(QQuickItem *k) {
    const QVariant v = k->property("rmppOrder");
    if (v.isValid()) return v.toInt();
    const char *cls = k->metaObject()->className();
    if (qstrncmp(cls, "BtSidebarItem", 13) == 0) return 100;
    if (qstrncmp(cls, "TsSidebarItem", 13) == 0) return 200;
    if (k->objectName() == QLatin1String("rmppHwrSidebarItem")) return 400;
    return -1;
}

void placeRow(QQuickItem *col, QQuickItem *row) {
    const int mine = railOrder(row);
    QQuickItem *after = nullptr, *wifi = nullptr;
    int best = -1;
    for (QQuickItem *k : col->childItems()) {
        if (k == row) continue;
        if (k->property("iconSource").toString().contains(QLatin1String("/wifi"))) wifi = k;
        const int o = railOrder(k);
        if (o >= 0 && o < mine && o > best) { best = o; after = k; }
    }
    if (!after) after = wifi;
    if (after) row->stackAfter(after);
}

void tryInject() {
    const auto windows = QGuiApplication::allWindows();
    for (QWindow *w : windows) {
        auto *qw = qobject_cast<QQuickWindow *>(w);
        if (!qw) continue;
        QQuickItem *col = findByName(qw->contentItem(), "settingsColumn");
        if (!col) col = findByName(qw, "settingsColumn");
        if (!col || col->property("_rmppts").toBool()) continue;
        col->setProperty("_rmppts", true);

        QQuickItem *settings = col;
        while (settings && settings->objectName() != QLatin1String("Settings")) settings = settings->parentItem();
        QQuickItem *loader = settings ? findByName(settings, "payloadLoader") : nullptr;
        QQmlEngine *eng = settings ? qmlEngine(settings) : nullptr;
        QQmlContext *base = settings ? qmlContext(settings) : nullptr;
        if (!settings || !loader || !eng || !base) {
            qWarning("[rmpp-tailscale] settings page found but parts missing");
            continue;
        }
        auto *ctx = new QQmlContext(base, settings);
        ctx->setContextProperty("ts", g_ts->map);
        ctx->setContextProperty("settingsRoot", settings);
        ctx->setContextProperty("payloadLoaderRef", loader);

        QQuickItem *row = createFrom(eng, ctx, "qrc:/rmppts/qml/TsSidebarItem.qml", col);
        if (row) placeRow(col, row);
        QQuickItem *page = createFrom(eng, ctx, "qrc:/rmppts/qml/TsPage.qml", settings);
        qWarning("[rmpp-tailscale] injected into Settings: row=%s page=%s", row ? "ok" : "FAILED", page ? "ok" : "FAILED");
    }
}

void rmpptsInit() {
    const QString rcc = qEnvironmentVariable("RMPPTS_RCC", QStringLiteral("/usr/lib/rmpp-tailscale/rmpp-tailscale.rcc"));
    const bool ok = QResource::registerResource(rcc);
    qWarning("[rmpp-tailscale] resources %s: %s", qPrintable(rcc), ok ? "registered" : "FAILED");
    auto *app = QCoreApplication::instance();
    g_ts = new Tailscale(app);
    auto *t = new QTimer(app);
    QObject::connect(t, &QTimer::timeout, app, [] { tryInject(); });
    t->start(1000);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(rmpptsInit)
