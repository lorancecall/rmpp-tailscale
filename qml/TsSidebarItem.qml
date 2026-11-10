// Sidebar row injected into the Settings page (after Bluetooth from rmpp-bt, else after Wi-Fi).
// Context properties: ts (QQmlPropertyMap), settingsRoot (Settings.qml root item).
import QtQuick
import QtQuick.Layouts
import ark.tokens as ArkTokens
import ark.controls as ArkControls

ArkControls.SidebarItem {
    readonly property int tsPage: 1002

    type: ArkTokens.Sidebar.primary
    text: settingsRoot.showLabels ? "Tailscale" : ""
    iconSource: "qrc:/rmppts/icons/tailscale.svg"
    highlighted: settingsRoot._selectedPage === tsPage
    focusPolicy: Qt.StrongFocus
    Layout.fillWidth: true

    onTriggered: settingsRoot._selectedPage = tsPage
}
