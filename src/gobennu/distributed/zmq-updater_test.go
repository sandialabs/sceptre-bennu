package distributed

import (
	"testing"
	"time"
)

// With no provider listening, a write must fail within the timeout instead of
// blocking forever, and the updater must stay usable (socket recreated).
func TestZMQUpdaterTimesOut(t *testing.T) {
	old := requestTimeout
	requestTimeout = 200 * time.Millisecond
	defer func() { requestTimeout = old }()

	u := NewZMQUpdater().(*ZMQUpdater)
	err := u.Init(Endpoint("tcp://127.0.0.1:1"))
	if err != nil {
		t.Fatal(err)
	}

	done := make(chan error, 1)
	go func() { done <- u.UpdateAnalogField("p", 1) }()

	select {
	case doneErr := <-done:
		if doneErr == nil {
			t.Fatal("expected timeout error")
		}
	case <-time.After(3 * time.Second):
		t.Fatal("update blocked past timeout")
	}

	if u.socket == nil {
		t.Fatal("socket not recreated after timeout")
	}
}
