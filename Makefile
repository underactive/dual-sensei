.PHONY: build flash monitor clean

build:
	pio run

flash:
	pio run -t upload

monitor:
	pio device monitor

clean:
	pio run -t clean
