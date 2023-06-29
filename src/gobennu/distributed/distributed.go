package distributed

import (
	"context"
)

type SubscriptionHandler chan PointData

type Subscriber interface {
	Init(...Option) error
	SubscribeToPoint(string, SubscriptionHandler) error
	Start(context.Context) error
}

type Updater interface {
	Init(...Option) error
	UpdateBinaryField(string, bool) error
	UpdateAnalogField(string, float64) error
	GetBatchUpdater() BatchUpdater
}

type BatchUpdater interface {
	UpdateBinaryField(string, bool)
	UpdateAnalogField(string, float64)
	Commit() error
}
