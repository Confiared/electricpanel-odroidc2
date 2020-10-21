electricpanel-odroidc2: EpollObject.cpp main.cpp Server.cpp Timer.cpp Timer/CheckInputs.cpp
	g++ -o electricpanel-odroidc2 -O2 EpollObject.cpp main.cpp Server.cpp Timer.cpp Timer/CheckInputs.cpp -lwiringPi -lwiringPiDev -lm -lpthread -lrt -lcrypt -std=c++11 -I.
