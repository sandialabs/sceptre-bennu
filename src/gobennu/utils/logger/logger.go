package logger

import (
	"github.com/activeshadow/logr/stdlogr"
	"github.com/go-logr/logr"
)

var (
	enabled   = true
	verbosity = 0

	DefaultLogger = stdlogr.New("sceptre", enabled, verbosity)
)

func Enable() {
	enabled = true
	DefaultLogger = stdlogr.New("sceptre", enabled, verbosity)
}

func Disable() {
	enabled = false
	DefaultLogger = stdlogr.New("sceptre", enabled, verbosity)
}

func Verbosity(level int) {
	verbosity = level
	DefaultLogger = stdlogr.New("sceptre", enabled, verbosity)
}

func Info(msg string, kvs ...interface{}) {
	DefaultLogger.Info(msg, kvs...)
}

func Error(err error, msg string, kvs ...interface{}) {
	DefaultLogger.Error(err, msg, kvs...)
}

func V(level int) logr.InfoLogger {
	return DefaultLogger.V(level)
}

func WithValues(kvs ...interface{}) logr.Logger {
	return DefaultLogger.WithValues(kvs...)
}

func WithName(name string) logr.Logger {
	return DefaultLogger.WithName(name)
}
