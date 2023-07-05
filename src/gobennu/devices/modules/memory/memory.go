package memory

import (
	"fmt"
	"strconv"
	"strings"
	"sync"

	"github.com/beevik/etree"
)

type Memory struct {
	// This mutex is used for locking the data manager as a whole to do things like
	// coordinate batch updates, etc.
	sync.Mutex

	// tag name --> IO module id
	binaryTags map[string]string
	analogTags map[string]string

	// IO module id --> device filter
	binaryModules map[string]string
	analogModules map[string]string

	// tag name --> new output value
	updatedBinaryTags map[string]bool
	updatedAnalogTags map[string]float64

	// tag name --> internal value (internal datastores)
	internalBinaryTags map[string]bool
	internalAnalogTags map[string]float64
	internalStringTags map[string]string

	// device filter --> status/value (external datastores)
	binaryDeviceStatuses map[string]bool
	analogDeviceValues   map[string]float64

	// Used during a data freeze to track updates coming in during the freeze.
	frozen bool

	// tag name --> new output value
	frozenUpdatedBinaryTags map[string]bool
	frozenUpdatedAnalogTags map[string]float64

	// tag name --> internal value (internal datastores)
	frozenInternalBinaryTags map[string]bool
	frozenInternalAnalogTags map[string]float64
	frozenInternalStringTags map[string]string

	// This mutex is used for locking access to the maps above. It's separate from
	// the embedded mutex since that one will be used by code external to this
	// struct to coordinate batch updates, etc.
	mutex sync.RWMutex
}

func NewMemory() *Memory {
	return &Memory{
		binaryModules:        make(map[string]string),
		analogModules:        make(map[string]string),
		binaryTags:           make(map[string]string),
		analogTags:           make(map[string]string),
		updatedBinaryTags:    make(map[string]bool),
		updatedAnalogTags:    make(map[string]float64),
		internalBinaryTags:   make(map[string]bool),
		internalAnalogTags:   make(map[string]float64),
		internalStringTags:   make(map[string]string),
		binaryDeviceStatuses: make(map[string]bool),
		analogDeviceValues:   make(map[string]float64),
	}
}

func (this *Memory) HandleTreeData(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "external-tag":
			if err := this.handleExternalTag(child); err != nil {
				return err
			}
		case "internal-tag":
			if err := this.handleInternalTag(child); err != nil {
				return err
			}
		}
	}

	return nil
}

func (this *Memory) handleExternalTag(e *etree.Element) error {
	var (
		name  string
		io    string
		ttype string
	)

	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "name":
			name = child.Text()
		case "io":
			io = child.Text()
		case "type":
			ttype = child.Text()
		}
	}

	switch ttype {
	case "binary":
		this.binaryTags[name] = io
	case "analog":
		this.analogTags[name] = io
	}

	return nil
}

func (this *Memory) handleInternalTag(e *etree.Element) error {
	var (
		name  string
		ttype string
		value interface{}
	)

	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "name":
			name = child.Text()
		case "status":
			ttype = "binary"

			if child.Text() == "true" {
				value = true
			} else {
				value = false
			}
		case "value":
			ttype = "analog"

			var err error

			value, err = strconv.ParseFloat(child.Text(), 64)
			if err != nil {
				return err
			}
		case "string":
			ttype = "string"
			value = child.Text()
		}
	}

	switch ttype {
	case "binary":
		this.internalBinaryTags[name] = value.(bool)
	case "analog":
		this.internalAnalogTags[name] = value.(float64)
	case "string":
		this.internalStringTags[name] = value.(string)
	}

	return nil
}

func (this *Memory) AddBinaryModule(id, device string) {
	this.mutex.Lock()
	defer this.mutex.Unlock()

	this.binaryModules[id] = device
}

func (this *Memory) AddAnalogModule(id, device string) {
	this.mutex.Lock()
	defer this.mutex.Unlock()

	this.analogModules[id] = device
}

func (this *Memory) HasTag(tag string) bool {
	this.mutex.RLock()
	defer this.mutex.RUnlock()

	if _, ok := this.binaryTags[tag]; ok {
		return true
	}

	if _, ok := this.analogTags[tag]; ok {
		return true
	}

	if _, ok := this.internalBinaryTags[tag]; ok {
		return true
	}

	if _, ok := this.internalAnalogTags[tag]; ok {
		return true
	}

	if _, ok := this.internalStringTags[tag]; ok {
		return true
	}

	return false
}

func (this *Memory) GetBinaryStatusForTag(tag string) (bool, error) {
	this.mutex.RLock()
	defer this.mutex.RUnlock()

	return this.getBinaryStatusForTag(tag)
}

func (this *Memory) getBinaryStatusForTag(tag string) (bool, error) {
	module, ok := this.binaryTags[tag]

	// backed by an IO module
	if ok {
		device, ok := this.binaryModules[module]
		if !ok {
			return false, fmt.Errorf("unknown IO module for tag %s", tag)
		}

		status, ok := this.getBinaryDeviceStatus(device)
		if !ok {
			return false, fmt.Errorf("unable to get status from IO module for tag %s", tag)
		}

		return status, nil
	}

	status, ok := this.internalBinaryTags[tag]

	if !ok {
		return false, fmt.Errorf("unknown tag %s", tag)
	}

	return status, nil
}

func (this *Memory) GetAnalogValueForTag(tag string) (float64, error) {
	this.mutex.RLock()
	defer this.mutex.RUnlock()

	return this.getAnalogValueForTag(tag)
}

func (this *Memory) getAnalogValueForTag(tag string) (float64, error) {
	module, ok := this.analogTags[tag]

	// backed by an IO module
	if ok {
		device, ok := this.analogModules[module]
		if !ok {
			return 0, fmt.Errorf("unknown IO module for tag %s", tag)
		}

		value, ok := this.getAnalogDeviceValue(device)
		if !ok {
			return 0, fmt.Errorf("unable to get value from IO module for tag %s", tag)
		}

		return value, nil
	}

	value, ok := this.internalAnalogTags[tag]

	if !ok {
		return 0, fmt.Errorf("unknown tag %s", tag)
	}

	return value, nil
}

func (this *Memory) GetStringValueForTag(tag string) (string, error) {
	this.mutex.RLock()
	defer this.mutex.RUnlock()

	value, ok := this.internalStringTags[tag]

	if !ok {
		return "", fmt.Errorf("unknown tag %s", tag)
	}

	return value, nil
}

func (this *Memory) UpdateBinaryTag(tag string, status bool) error {
	this.mutex.Lock()
	defer this.mutex.Unlock()

	if _, ok := this.binaryTags[tag]; ok {
		if this.frozen {
			this.frozenUpdatedBinaryTags[tag] = status
		} else {
			this.updatedBinaryTags[tag] = status
		}

		return nil
	}

	if _, ok := this.internalBinaryTags[tag]; ok {
		if this.frozen {
			this.frozenInternalBinaryTags[tag] = status
		} else {
			this.internalBinaryTags[tag] = status
		}

		return nil
	}

	return fmt.Errorf("binary tag %s does not exist", tag)
}

func (this *Memory) UpdateAnalogTag(tag string, value float64) error {
	this.mutex.Lock()
	defer this.mutex.Unlock()

	if _, ok := this.analogTags[tag]; ok {
		if this.frozen {
			this.frozenUpdatedAnalogTags[tag] = value
		} else {
			this.updatedAnalogTags[tag] = value
		}

		return nil
	}

	if _, ok := this.internalAnalogTags[tag]; ok {
		if this.frozen {
			this.frozenInternalAnalogTags[tag] = value
		} else {
			this.internalAnalogTags[tag] = value
		}

		return nil
	}

	return fmt.Errorf("analog tag %s does not exist", tag)
}

func (this *Memory) UpdateStringTag(tag string, value string) error {
	this.mutex.Lock()
	defer this.mutex.Unlock()

	if _, ok := this.internalStringTags[tag]; ok {
		if this.frozen {
			this.frozenInternalStringTags[tag] = value
		} else {
			this.internalStringTags[tag] = value
		}

		return nil
	}

	return fmt.Errorf("string tag %s does not exist", tag)
}

func (this *Memory) UpdateBinaryDevices(yield func(string, bool) error) error {
	this.mutex.Lock()
	defer this.mutex.Unlock()

	var updateErr error

	for tag, status := range this.updatedBinaryTags {
		module, ok := this.binaryTags[tag]
		if !ok {
			continue // TODO: more to do here?
		}

		device, ok := this.binaryModules[module]
		if !ok {
			continue // TODO: more to do here?
		}

		current, err := this.getBinaryStatusForTag(tag)
		if err != nil {
			continue // TODO: more to do here?
		}

		// Only update binary devices if the status has actually changed.
		if status == current {
			delete(this.updatedBinaryTags, tag)
			continue
		}

		if err := yield(device, status); err != nil {
			updateErr = err
		} else {
			this.setBinaryDeviceStatus(device, status)
			delete(this.updatedBinaryTags, tag)
		}
	}

	// Since we don't check to see if `updateErr` is nil before setting it in the
	// for-loop above, we will always be returning the last yield error encountered
	// (assuming there were multiple yield errors).
	return updateErr
}

func (this *Memory) UpdateAnalogDevices(yield func(string, float64) error) error {
	this.mutex.Lock()
	defer this.mutex.Unlock()

	var updateErr error

	for tag, value := range this.updatedAnalogTags {
		module, ok := this.analogTags[tag]
		if !ok {
			continue // TODO: more to do here?
		}

		device, ok := this.analogModules[module]
		if !ok {
			continue // TODO: more to do here?
		}

		current, err := this.getAnalogValueForTag(tag)
		if err != nil {
			continue // TODO: more to do here?
		}

		// Only update analog devices if the value has actually changed.
		if value == current {
			delete(this.updatedAnalogTags, tag)
			continue
		}

		if err := yield(device, value); err != nil {
			updateErr = err
		} else {
			this.setAnalogDeviceValue(device, value)
			delete(this.updatedAnalogTags, tag)
		}
	}

	// Since we don't check to see if `updateErr` is nil before setting it in the
	// for-loop above, we will always be returning the last yield error encountered
	// (assuming there were multiple yield errors).
	return updateErr
}

func (this *Memory) Freeze() error {
	this.Lock()
	defer this.Unlock()

	this.frozen = true

	this.frozenUpdatedBinaryTags = make(map[string]bool)
	this.frozenUpdatedAnalogTags = make(map[string]float64)
	this.frozenInternalBinaryTags = make(map[string]bool)
	this.frozenInternalAnalogTags = make(map[string]float64)
	this.frozenInternalStringTags = make(map[string]string)

	return nil
}

func (this *Memory) Unfreeze() error {
	this.Lock()
	defer this.Unlock()

	for k, v := range this.frozenUpdatedBinaryTags {
		this.updatedBinaryTags[k] = v
	}

	for k, v := range this.frozenUpdatedAnalogTags {
		this.updatedAnalogTags[k] = v
	}

	for k, v := range this.frozenInternalBinaryTags {
		this.internalBinaryTags[k] = v
	}

	for k, v := range this.frozenInternalAnalogTags {
		this.internalAnalogTags[k] = v
	}

	for k, v := range this.frozenInternalStringTags {
		this.internalStringTags[k] = v
	}

	this.frozen = false

	return nil
}

func (this *Memory) AddBinaryDevice(device string) {
	this.mutex.Lock()
	defer this.mutex.Unlock()

	this.binaryDeviceStatuses[device] = false
}

func (this *Memory) AddAnalogDevice(device string) {
	this.mutex.Lock()
	defer this.mutex.Unlock()

	this.analogDeviceValues[device] = 0.0
}

func (this *Memory) IsBinaryDevice(device string) bool {
	this.mutex.RLock()
	defer this.mutex.RUnlock()

	return this.isBinaryDevice(device)
}

func (this *Memory) isBinaryDevice(device string) bool {
	_, ok := this.binaryDeviceStatuses[device]
	return ok
}

func (this *Memory) IsAnalogDevice(device string) bool {
	this.mutex.RLock()
	defer this.mutex.RUnlock()

	return this.isAnalogDevice(device)
}

func (this *Memory) isAnalogDevice(device string) bool {
	_, ok := this.analogDeviceValues[device]
	return ok
}

func (this *Memory) GetBinaryDeviceStatus(device string) (bool, bool) {
	this.mutex.RLock()
	defer this.mutex.RUnlock()

	return this.getBinaryDeviceStatus(device)
}

func (this *Memory) getBinaryDeviceStatus(device string) (bool, bool) {
	status, ok := this.binaryDeviceStatuses[device]
	return status, ok
}

func (this *Memory) GetAnalogDeviceValue(device string) (float64, bool) {
	this.mutex.RLock()
	defer this.mutex.RUnlock()

	return this.getAnalogDeviceValue(device)
}

func (this *Memory) getAnalogDeviceValue(device string) (float64, bool) {
	value, ok := this.analogDeviceValues[device]
	return value, ok
}

func (this *Memory) SetBinaryDeviceStatus(device string, status bool) error {
	this.mutex.Lock()
	defer this.mutex.Unlock()

	return this.setBinaryDeviceStatus(device, status)
}

func (this *Memory) setBinaryDeviceStatus(device string, status bool) error {
	if !this.isBinaryDevice(device) {
		return fmt.Errorf("binary device does not exist")
	}

	this.binaryDeviceStatuses[device] = status
	return nil
}

func (this *Memory) SetAnalogDeviceValue(device string, value float64) error {
	this.mutex.Lock()
	defer this.mutex.Unlock()

	return this.setAnalogDeviceValue(device, value)
}

func (this *Memory) setAnalogDeviceValue(device string, value float64) error {
	if !this.isAnalogDevice(device) {
		return fmt.Errorf("analog device does not exist")
	}

	this.analogDeviceValues[device] = value
	return nil
}

func (this *Memory) GetBinaryPoints() (map[string]bool, error) {
	this.mutex.RLock()
	defer this.mutex.RUnlock()

	return this.getBinaryPoints()
}

func (this *Memory) getBinaryPoints() (map[string]bool, error) {
	var (
		points = make(map[string]bool)
		errors []string
	)

	for tag := range this.binaryTags {
		status, err := this.GetBinaryStatusForTag(tag)
		if err != nil {
			errors = append(errors, fmt.Sprintf("getting status for %s", tag))
			continue
		}

		points[tag] = status
	}

	for tag, status := range this.internalBinaryTags {
		points[tag] = status
	}

	if errors != nil {
		return points, fmt.Errorf(strings.Join(errors, ", "))
	}

	return points, nil
}

func (this *Memory) GetAnalogPoints() (map[string]float64, error) {
	this.mutex.RLock()
	defer this.mutex.RUnlock()

	return this.getAnalogPoints()
}

func (this *Memory) getAnalogPoints() (map[string]float64, error) {
	var (
		points = make(map[string]float64)
		errors []string
	)

	for tag := range this.analogTags {
		value, err := this.GetAnalogValueForTag(tag)
		if err != nil {
			errors = append(errors, fmt.Sprintf("getting value for %s", tag))
			continue
		}

		points[tag] = value
	}

	for tag, value := range this.internalAnalogTags {
		points[tag] = value
	}

	if errors != nil {
		return points, fmt.Errorf(strings.Join(errors, ", "))
	}

	return points, nil
}

func (this *Memory) GetPoints() (map[string]interface{}, error) {
	this.mutex.RLock()
	defer this.mutex.RUnlock()

	var (
		points = make(map[string]interface{})
		errors []string
	)

	statuses, err := this.getBinaryPoints()
	if err != nil {
		errors = append(errors, err.Error())
	}

	for tag, status := range statuses {
		points[tag] = status
	}

	values, err := this.getAnalogPoints()
	if err != nil {
		errors = append(errors, err.Error())
	}

	for tag, value := range values {
		points[tag] = value
	}

	if errors != nil {
		return points, fmt.Errorf(strings.Join(errors, ", "))
	}

	return points, nil
}

func (this *Memory) GetDevicePoints() []string {
	this.mutex.RLock()
	defer this.mutex.RUnlock()

	var points []string

	for p := range this.binaryDeviceStatuses {
		points = append(points, p)
	}

	for p := range this.analogDeviceValues {
		points = append(points, p)
	}

	return points
}
