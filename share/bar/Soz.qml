// The soz switch in the Omarchy bar: "TR", dimmed while Superwhisper owns the
// dictation keys, lit while Turkish dictation does, and in the urgent color
// while it records. One click turns Turkish dictation on or off.
import QtQuick
import Quickshell
import Quickshell.Io
import qs.Ui
import qs.Commons

BarWidget {
  id: root
  moduleName: "soz"

  readonly property string runtimeDir: (Quickshell.env("XDG_RUNTIME_DIR") || "/run/user/1000") + "/soz"

  // Both files are tiny and written by soz; anything else in them is ignored.
  readonly property bool enabled: String(enabledFile.text()).trim() === "1"
  readonly property string phase: {
    var value = String(phaseFile.text()).trim()
    return ["loading", "idle", "recording", "transcribing", "error"].indexOf(value) >= 0 ? value : ""
  }
  // Set on click and cleared when the file catches up, so the icon answers
  // the click immediately rather than when the switch finishes.
  property var pending: null
  readonly property bool shownOn: pending !== null ? pending : enabled

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  onEnabledChanged: pending = null

  FileView {
    id: enabledFile
    path: root.runtimeDir + "/enabled"
    watchChanges: true
    printErrors: false
    blockLoading: true
    onFileChanged: reload()
  }

  FileView {
    id: phaseFile
    path: root.runtimeDir + "/phase"
    watchChanges: true
    printErrors: false
    blockLoading: true
    onFileChanged: reload()
  }

  // A file that does not exist yet has nothing to watch; look again now and
  // then so the first `soz on` from a terminal still shows up.
  Timer {
    interval: 2000
    running: true
    repeat: true
    onTriggered: { enabledFile.reload(); phaseFile.reload() }
  }

  Timer {
    id: pendingTimeout
    interval: 4000
    onTriggered: root.pending = null
  }

  WidgetButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: "TR"
    fontFamily: root.bar ? root.bar.fontFamily : Style.font.family
    dimmed: !root.shownOn
    active: root.shownOn && (root.phase === "recording" || root.phase === "error")
    tooltipText: {
      if (!root.shownOn) return "Turkish dictation is off. Click to turn it on."
      if (root.phase === "recording") return "Recording Turkish…"
      if (root.phase === "transcribing") return "Transcribing…"
      if (root.phase === "loading") return "Turkish dictation is loading the model…"
      if (root.phase === "error") return "Turkish dictation has a problem. Run `soz status`."
      return "Turkish dictation is on (Alt + Space, Right Alt). Click to turn it off."
    }
    onPressed: function(b) {
      if (b !== Qt.LeftButton) return
      root.pending = !root.shownOn
      pendingTimeout.restart()
      root.bar.run("soz switch")
    }
  }
}
