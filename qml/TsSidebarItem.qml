// Sidebar row injected into the Settings page, placed by the order shared by the rmpp add-ons (rmppOrder).
// Context properties: ts (QQmlPropertyMap), settingsRoot (Settings.qml root item).
import QtQuick
import QtQuick.Layouts
import ark.tokens as ArkTokens
import ark.controls as ArkControls

ArkControls.SidebarItem {
    readonly property int tsPage: 1002
    // Settings rail order of the rmpp add-ons: rmpp-bt 100, rmpp-tailscale 200, rmpp-lang 300, rmpp-hwr 400.
    property int rmppOrder: 200

    type: ArkTokens.Sidebar.primary
    text: settingsRoot.showLabels ? "Tailscale" : ""
    iconSource: "qrc:/rmppts/icons/tailscale.svg"
    highlighted: settingsRoot._selectedPage === tsPage
    focusPolicy: Qt.StrongFocus
    Layout.fillWidth: true

    onTriggered: settingsRoot._selectedPage = tsPage
}
