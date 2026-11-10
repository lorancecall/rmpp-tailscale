// Tailscale settings page, shown over the Settings payload area when its sidebar row is selected.
// Context properties: ts (QQmlPropertyMap), settingsRoot, payloadLoaderRef.
import QtQuick
import QtQuick.Layouts
import ark.tokens as ArkTokens
import ark.controls as ArkControls

Rectangle {
    id: page
    readonly property int tsPage: 1002
    readonly property var peers: {
        try { return JSON.parse(ts.peers || "[]"); } catch (e) { return []; }
    }
    readonly property bool running: ts.state === "Running"
    readonly property bool needsLogin: !!ts.enabled && !!ts.daemon && (ts.state === "NeedsLogin" || ts.state === "NoState")
    readonly property string stateText: {
        if (!ts.installed) return "Tailscale is not installed";
        if (!ts.enabled) return "Off";
        if (!ts.daemon) return "Starting…";
        switch (ts.state) {
        case "Running": return "Connected" + (ts.ip4 ? " · " + ts.ip4 : "");
        case "NeedsLogin": return "Log in to connect this tablet";
        case "NeedsMachineAuth": return "Waiting for an admin to approve this device";
        case "Starting": return "Connecting…";
        case "Stopped": return "Disconnected";
        default: return ts.state || "Starting…";
        }
    }

    function send(cmd) { ts.command = cmd + "#" + Date.now(); }

    x: payloadLoaderRef.x
    y: payloadLoaderRef.y
    width: payloadLoaderRef.width
    height: payloadLoaderRef.height
    z: 10
    color: "white"
    visible: settingsRoot._selectedPage === tsPage
    onVisibleChanged: send(visible ? "active:1" : "active:0")

    ArkControls.PlaceholderBar {
        id: placeholder
        anchors { top: parent.top; left: parent.left; right: parent.right }
    }

    Flickable {
        anchors {
            top: placeholder.bottom; left: parent.left; right: parent.right; bottom: parent.bottom
            leftMargin: ArkControls.Values.margin.horizontal.large
            rightMargin: ArkControls.Values.margin.horizontal.large
        }
        contentHeight: column.implicitHeight + ArkControls.Values.margin.vertical.large
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: column
            width: parent.width
            spacing: ArkControls.Values.spacing.large

            ArkControls.Title {
                type: ArkControls.Title.Large
                text: "Tailscale"
                Layout.fillWidth: true
            }

            ArkControls.Panel {
                label: "Tailscale"
                description: page.stateText
                iconSource: "qrc:/rmppts/icons/tailscale.svg"
                Layout.fillWidth: true
                action: ArkControls.Toggle {
                    checked: !!ts.enabled
                    enabled: !!ts.installed
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -20
                        onClicked: page.send(ts.enabled ? "power:0" : "power:1")
                    }
                }
            }

            ArkControls.Panel {
                visible: !!ts.status
                label: ts.status || ""
                Layout.fillWidth: true
            }

            // ---- Login ----
            ArkControls.Panel {
                visible: page.needsLogin && !ts.authUrl
                label: "Log in to Tailscale"
                description: "You will get a QR code to scan with your phone"
                Layout.fillWidth: true
                action: ArkControls.Button {
                    text: "Log in"
                    type: ArkTokens.Button.secondary
                    enabled: !ts.busy
                    onClicked: page.send("login")
                }
            }

            ColumnLayout {
                visible: page.needsLogin && !!ts.authUrl
                spacing: ArkControls.Values.spacing.medium
                Layout.fillWidth: true

                ArkControls.Title {
                    type: ArkControls.Title.Medium
                    text: "Scan to log in"
                    Layout.fillWidth: true
                }
                ArkControls.Panel {
                    label: "Scan this code with your phone's camera and log in with your Tailscale account."
                    description: "Or open this address on any device: " + (ts.authUrl || "")
                    Layout.fillWidth: true
                }
                Image {
                    source: ts.qr || ""
                    Layout.preferredWidth: 480
                    Layout.preferredHeight: 480
                    Layout.alignment: Qt.AlignHCenter
                    fillMode: Image.PreserveAspectFit
                    smooth: false
                }
                ArkControls.Button {
                    text: "Cancel"
                    type: ArkTokens.Button.secondary
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: page.send("cancel")
                }
            }

            // ---- Connected ----
            ArkControls.Panel {
                visible: !!ts.enabled && ts.state === "Stopped"
                label: "Tailscale is disconnected"
                Layout.fillWidth: true
                action: ArkControls.Button {
                    text: "Connect"
                    type: ArkTokens.Button.secondary
                    enabled: !ts.busy
                    onClicked: page.send("power:1")
                }
            }

            ArkControls.Title {
                visible: page.running
                type: ArkControls.Title.Medium
                text: "This tablet"
                Layout.fillWidth: true
            }
            ArkControls.Panel {
                visible: page.running
                label: ts.dnsName || ts.hostname || ""
                description: [ts.ip4, ts.ip6].filter(s => !!s).join("   ")
                Layout.fillWidth: true
            }
            ArkControls.Panel {
                visible: page.running && !!ts.ip4
                label: "SSH from your computer"
                description: "ssh root@" + (ts.ip4 || "")
                Layout.fillWidth: true
            }
            ArkControls.Panel {
                visible: page.running || ts.state === "Stopped"
                label: ts.user || "Account"
                description: ts.tailnet ? "Tailnet: " + ts.tailnet : ""
                Layout.fillWidth: true
                action: ArkControls.Button {
                    text: "Log out"
                    type: ArkTokens.Button.secondary
                    enabled: !ts.busy
                    onClicked: page.send("logout")
                }
            }

            ArkControls.Title {
                visible: page.running && page.peers.length > 0
                type: ArkControls.Title.Medium
                text: "Devices on your tailnet"
                Layout.fillWidth: true
            }
            Repeater {
                model: page.running ? page.peers : []
                delegate: ArkControls.Panel {
                    required property var modelData
                    label: modelData.name
                    description: (modelData.online ? "Online" : "Offline") + (modelData.ip ? " · " + modelData.ip : "")
                                 + (modelData.os ? " · " + modelData.os : "")
                    Layout.fillWidth: true
                }
            }

            ArkControls.Title {
                visible: !!ts.installed
                type: ArkControls.Title.Medium
                text: "Software"
                Layout.fillWidth: true
            }
            ArkControls.Panel {
                visible: !!ts.installed
                label: "Tailscale " + (ts.version || "")
                description: ts.updateStatus
                             || (ts.updateAvailable ? "Version " + ts.latest + " is available"
                                 : (ts.latest ? "Up to date" : ""))
                Layout.fillWidth: true
                action: ArkControls.Button {
                    text: ts.updateAvailable ? "Update" : "Check for updates"
                    type: ArkTokens.Button.secondary
                    enabled: !ts.busy && !ts.checking
                    onClicked: page.send(ts.updateAvailable ? "update" : "checkUpdate")
                }
            }
            ArkControls.Panel {
                visible: !!ts.installed
                label: "About"
                description: "Userspace networking: other devices can reach this tablet (SSH included); "
                             + "the tablet reaches them through a SOCKS5 proxy on localhost:1055."
                Layout.fillWidth: true
            }
        }
    }
}
