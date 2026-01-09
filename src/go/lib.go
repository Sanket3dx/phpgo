package main

/*
#include <stdlib.h>
typedef void (*callback_t)(long long id);

static void call_callback(callback_t cb, long long id) {
	cb(id);
}
*/
import "C"

import (
	"reflect"
	"sync"
	"unsafe"
)

// Registry for Channels and WaitGroups
var (
	chanMap  = make(map[int64]chan unsafe.Pointer)
	wgMap    = make(map[int64]*sync.WaitGroup)
	mapMutex sync.RWMutex
	nextID   int64 = 1
)

func registerChannel(ch chan unsafe.Pointer) int64 {
	mapMutex.Lock()
	defer mapMutex.Unlock()
	id := nextID
	nextID++
	chanMap[id] = ch
	return id
}

func getChannel(id int64) (chan unsafe.Pointer, bool) {
	mapMutex.RLock()
	defer mapMutex.RUnlock()
	ch, ok := chanMap[id]
	return ch, ok
}

func registerWG(wg *sync.WaitGroup) int64 {
	mapMutex.Lock()
	defer mapMutex.Unlock()
	id := nextID
	nextID++
	wgMap[id] = wg
	return id
}

func getWG(id int64) (*sync.WaitGroup, bool) {
	mapMutex.RLock()
	defer mapMutex.RUnlock()
	wg, ok := wgMap[id]
	return wg, ok
}

//export PG_Lock
func PG_Lock() {
	mapMutex.Lock()
}

//export PG_Unlock
func PG_Unlock() {
	mapMutex.Unlock()
}

//export PG_Ping
func PG_Ping() int {
	return 1
}

//export PG_MakeChannel
func PG_MakeChannel(buffer C.int) int64 {
	// If buffer < 0, unbuffered
	bf := int(buffer)
	if bf < 0 {
		bf = 0
	}
	var ch chan unsafe.Pointer
	ch = make(chan unsafe.Pointer, bf)
	return registerChannel(ch)
}

//export PG_ChanSend
func PG_ChanSend(id int64, val unsafe.Pointer) int {
	ch, ok := getChannel(id)
	if !ok {
		return 0 // False/Error
	}
	// Handle panic (send on closed channel)
	defer func() {
		if r := recover(); r != nil {
			// In a real generic lib, we might want to signal this specifically
		}
	}()

	ch <- val
	return 1
}

//export PG_ChanRecv
func PG_ChanRecv(id int64) unsafe.Pointer {
	ch, ok := getChannel(id)
	if !ok {
		return nil
	}
	val, open := <-ch
	if !open {
		return nil // We need a way to signal closed vs nil value.
		// For MVP, assume nil is "closed or nil"
		// If we wanted exact parity, we'd need a status return.
	}
	return val
}

//export PG_ChanClose
func PG_ChanClose(id int64) {
	ch, ok := getChannel(id)
	if ok {
		// potential panic if already closed
		defer func() { recover() }()
		close(ch)
	}
}

//export PG_MakeWaitGroup
func PG_MakeWaitGroup() int64 {
	wg := &sync.WaitGroup{}
	return registerWG(wg)
}

//export PG_WGAdd
func PG_WGAdd(id int64, delta int) {
	wg, ok := getWG(id)
	if ok {
		wg.Add(delta)
	}
}

//export PG_WGDone
func PG_WGDone(id int64) {
	wg, ok := getWG(id)
	if ok {
		wg.Done()
	}
}

//export PG_WGWait
func PG_WGWait(id int64) {
	wg, ok := getWG(id)
	if ok {
		wg.Wait()
	}
}

// ----------- Select Implementation -----------

// cases: array of (type, channel_id, send_val)
// returns: (index, recv_val)
// We need a complex struct passing.
// For simplicity in MVP C-binding, let's assume specific signatures for now
// or maybe we implement a "Select Builder" pattern in C calling Go.
// Let's try a dynamic Select with fixed arrays passed from C?
// Pointers to arrays usually work.
//
//export PG_Select
func PG_Select(count int, types *C.int, chanIds *C.longlong, values *unsafe.Pointer, retIdx *C.int, retVal *unsafe.Pointer) int {
	// types: 0=Recv, 1=Send, 2=Default
	// We build []reflect.SelectCase

	// Convert C arrays to Go slices (unsafe arithmetic)
	// Usage of unsafe to iterate pointers

	// Slice headers
	var typeSlice []C.int
	typeHdr := (*reflect.SliceHeader)(unsafe.Pointer(&typeSlice))
	typeHdr.Data = uintptr(unsafe.Pointer(types))
	typeHdr.Len = count
	typeHdr.Cap = count

	var idSlice []C.longlong
	idHdr := (*reflect.SliceHeader)(unsafe.Pointer(&idSlice))
	idHdr.Data = uintptr(unsafe.Pointer(chanIds))
	idHdr.Len = count
	idHdr.Cap = count

	var valSlice []unsafe.Pointer
	valHdr := (*reflect.SliceHeader)(unsafe.Pointer(&valSlice))
	valHdr.Data = uintptr(unsafe.Pointer(values))
	valHdr.Len = count
	valHdr.Cap = count

	cases := make([]reflect.SelectCase, count)

	for i := 0; i < count; i++ {
		t := int(typeSlice[i])
		chid := int64(idSlice[i])

		if t == 2 { // Default
			cases[i] = reflect.SelectCase{Dir: reflect.SelectDefault}
		} else {
			ch, ok := getChannel(chid)
			if !ok {
				// Invalid channel, maybe panic or handle?
				// For select, if channel is nil, it blocks forever.
				// We'll treat missing ID as nil channel (block)
				cases[i] = reflect.SelectCase{Dir: reflect.SelectRecv} // Just a dummy that blocks if ch is nil
			}

			if t == 0 { // Recv
				cases[i] = reflect.SelectCase{Dir: reflect.SelectRecv, Chan: reflect.ValueOf(ch)}
			} else if t == 1 { // Send
				cases[i] = reflect.SelectCase{Dir: reflect.SelectSend, Chan: reflect.ValueOf(ch), Send: reflect.ValueOf(valSlice[i])}
			}
		}
	}

	chosen, recv, recvOK := reflect.Select(cases)

	*retIdx = C.int(chosen)
	if recv.IsValid() {
		// recv is a reflect.Value holding the unsafe.Pointer
		// We need to extract the pointer and put it in *retVal
		ptr, _ := recv.Interface().(unsafe.Pointer)
		*retVal = ptr
	} else {
		*retVal = nil
	}

	if !recvOK && cases[chosen].Dir == reflect.SelectRecv {
		return 0 // Channel closed
	}

	return 1
}

// ----------- Goroutines -----------

// We need a callback function pointer from C
var phpCallback C.callback_t

//export PG_SetCallback
func PG_SetCallback(cb C.callback_t) {
	phpCallback = cb
}

//export PG_StartGoroutine
func PG_StartGoroutine(cbId C.longlong) {
	go func() {
		// Call back into C
		// The C function will have to attach to the PHP thread or similar.
		if phpCallback != nil {
			C.call_callback(phpCallback, cbId)
		}
	}()
}

func main() {}
