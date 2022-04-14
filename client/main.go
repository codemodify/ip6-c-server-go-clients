package main

import (
	"fmt"
	"log"
	"net"
	"sync"
	"time"
)

func main() {
	wg := &sync.WaitGroup{}

	for i := 0; i < 5; i++ {
		wg.Add(1)
		connectThenSendAndRecv(wg)
	}

	wg.Wait()
}

func connectThenSendAndRecv(wg *sync.WaitGroup) {
	//
	conn, err := net.Dial("tcp6", "[::1]:8888")
	checkErr(err)

	//
	time.Sleep(1 * time.Second)
	fmt.Println("send: ping")
	_, err = conn.Write([]byte("ping"))
	checkErr(err)

	//
	time.Sleep(1 * time.Second)
	reply := make([]byte, 1024)
	_, err = conn.Read(reply)
	checkErr(err)
	fmt.Println("reply:", string(reply))

	//
	time.Sleep(1 * time.Second)
	defer conn.Close()

	wg.Done()
}

func checkErr(err error) {
	if err != nil {
		log.Fatal(err)
	}
}
