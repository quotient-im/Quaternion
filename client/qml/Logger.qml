import QtQml 2.15
import QtQuick 2.15

// This component can be used either as a logging category (pass it to
// console.log() or any other console method) or as a basic logger itself:
// if you only need log/warn/error kind of things you can save a few keystrokes
// by writing logger.warn(...) instead of console.warn(logger, ...)
LoggingCategory {
    name: 'quaternion.timeline.qml'
    defaultLogLevel: LoggingCategory.Info

    function debug() { return console.log(this, arguments) }
    function log() { return debug(arguments) }
    function warn() { return console.warn(this, arguments) }
    function error() { return console.error(this, arguments) }
}
