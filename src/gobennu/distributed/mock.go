// +build ignore
package distributed

import (
	"context"

	"github.com/stretchr/testify/mock"
)

type MockSubscriber struct {
	mock.Mock

	handlers map[string][]SubscriptionHandler
}

func NewMockSubscriber() *MockSubscriber {
	return &MockSubscriber{
		handlers: make(map[string][]SubscriptionHandler),
	}
}

func (this *MockSubscriber) Init(...Option) error {
	return nil
}

func (this *MockSubscriber) SubscribeToDevice(filter string, handler SubscriptionHandler) error {
	this.handlers[filter] = append(this.handlers[filter], handler)

	args := this.Called(filter, handler)
	return args.Error(0)
}

func (this *MockSubscriber) Start(context.Context) error {
	return nil
}

func (this *MockSubscriber) SimulatePublish(filter string, data DeviceData) {
	for _, handler := range this.handlers[filter] {
		handler <- data
	}
}

type MockUpdater struct {
	mock.Mock
}

func (this *MockUpdater) Init(...Option) error {
	return nil
}

func (this *MockUpdater) UpdateBinaryField(device Device, status bool) error {
	args := this.Called(device, status)
	return args.Error(0)
}

func (this *MockUpdater) UpdateAnalogField(device Device, value float64) error {
	args := this.Called(device, value)
	return args.Error(0)
}

func (this *MockUpdater) GetBatchUpdater() BatchUpdater {
	args := this.Called()
	return args.Get(0).(*MockBatchUpdater)
}

type MockBatchUpdater struct {
	*MockUpdater
}

func (this *MockBatchUpdater) UpdateBinaryField(device Device, status bool) {
	this.Called(device, status)
}

func (this *MockBatchUpdater) UpdateAnalogField(device Device, value float64) {
	this.Called(device, value)
}

func (this *MockBatchUpdater) Commit() error {
	args := this.Called()
	return args.Error(0)
}
