################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../src/Application.o \
../src/EmulNet.o \
../src/Log.o \
../src/MP1Node.o \
../src/Member.o \
../src/Params.o 

CPP_SRCS += \
../src/Application.cpp \
../src/EmulNet.cpp \
../src/Log.cpp \
../src/MP1Node.cpp \
../src/Member.cpp \
../src/Params.cpp 

OBJS += \
./src/Application.o \
./src/EmulNet.o \
./src/Log.o \
./src/MP1Node.o \
./src/Member.o \
./src/Params.o 

CPP_DEPS += \
./src/Application.d \
./src/EmulNet.d \
./src/Log.d \
./src/MP1Node.d \
./src/Member.d \
./src/Params.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	/opt/local/bin/g++-mp-4.7 -std=c++0x -O0 -g3 -Wall -c -fmessage-length=0 -std=c++11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


