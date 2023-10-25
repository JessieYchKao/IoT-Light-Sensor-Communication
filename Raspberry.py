import socket
import threading
import RPi.GPIO as GPIO
import time
import struct
import sys

UDP_IP = "192.168.0.167"
UDP_PORT = 5005
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
low_threshold = 400
high_threshold = 700
last_msg = time.time()
need_flash = False

lock = threading.Lock()

def receive():
    while True:
        data, addr = sock.recvfrom(1024)
        received = data.decode()
        change_LED(int(received))
        global last_msg
        with lock:
            last_msg = time.time()
        print("Received message: %s" % data.decode())

def packet_lost_detect():
    while True:
        global last_msg
        cur_time = time.time()

        dif = cur_time - last_msg
        if dif > 10:
            print("Not receiving data from ESP8266 for over 10 seconds...")
            global need_flash
            with lock:
                need_flash = True
            # Chech if the button is pressed
            while GPIO.input(10) != GPIO.HIGH:
                time.sleep(0.1)
            print("Reset systems")
            sock.sendto('Reset'.encode(), (UDP_IP, UDP_PORT))
            # turn off 4 LEDs
            with lock:
                need_flash = False
            time.sleep(1)
            GPIO.output(11, GPIO.LOW)
            GPIO.output(12, GPIO.LOW)
            GPIO.output(13, GPIO.LOW)
            GPIO.output(15, GPIO.LOW)
            # Chech if the button is pressed
            while GPIO.input(10) != GPIO.HIGH:
                time.sleep(0.1)
            GPIO.output(12, GPIO.HIGH)
            sock.sendto('Start'.encode(), (UDP_IP, UDP_PORT))
            print("Start collecting data...")
            last_msg = time.time()

def flash_LED():
    while True:
        if need_flash:
            GPIO.output(12, GPIO.LOW)
            time.sleep(0.25)
            GPIO.output(12, GPIO.HIGH)
            time.sleep(0.25)

def change_LED(val):
    if val > high_threshold:
        GPIO.output(11, GPIO.HIGH)
        GPIO.output(13, GPIO.HIGH)
        GPIO.output(15, GPIO.HIGH)
    elif val < low_threshold:
        GPIO.output(11, GPIO.HIGH)
        GPIO.output(13, GPIO.LOW)
        GPIO.output(15, GPIO.LOW)
    else:
        GPIO.output(11, GPIO.HIGH)
        GPIO.output(13, GPIO.HIGH)
        GPIO.output(15, GPIO.LOW)

def main():
    GPIO.setmode(GPIO.BOARD)
    # button
    GPIO.setup(10, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
    # white LED
    GPIO.setup(12, GPIO.OUT)
    GPIO.output(12, GPIO.LOW)

    # other LEDs
    GPIO.setup(11, GPIO.OUT)
    GPIO.setup(13, GPIO.OUT)
    GPIO.setup(15, GPIO.OUT)
    GPIO.output(11, GPIO.LOW)
    GPIO.output(13, GPIO.LOW)
    GPIO.output(15, GPIO.LOW)

    # Chech if the button is pressed
    while GPIO.input(10) != GPIO.HIGH:
        time.sleep(0.1)
    GPIO.output(12, GPIO.HIGH)
    sock.sendto('Start'.encode(), (UDP_IP, UDP_PORT))
    print("Start collecting data...")
    global last_msg
    with lock:
        last_msg = time.time()
    print("last_msg setup: %d" % last_msg)

    receiving = threading.Thread(target=receive)
    receiving.start()

    packet = threading.Thread(target=packet_lost_detect)
    packet.start()

    flashing = threading.Thread(target=flash_LED)
    flashing.start()

main()