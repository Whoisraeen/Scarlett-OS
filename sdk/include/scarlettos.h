/**
 * @file scarlettos.h
 * @brief ScarlettOS SDK - Main Header
 * 
 * Primary include file for ScarlettOS application development
 */

#ifndef SCARLETTOS_SDK_H
#define SCARLETTOS_SDK_H

// Version information
#define SCARLETTOS_VERSION_MAJOR 0
#define SCARLETTOS_VERSION_MINOR 1
#define SCARLETTOS_VERSION_PATCH 0
#define SCARLETTOS_VERSION "0.1.0"

// Core system headers
#include "scarlettos/types.h"
#include "scarlettos/syscall.h"
#include "scarlettos/ipc.h"
#include "scarlettos/memory.h"
#include "scarlettos/process.h"
#include "scarlettos/thread.h"
#include "scarlettos/file.h"
#include "scarlettos/network.h"
#include "scarlettos/gui.h"

// Standard library compatibility
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#endif // SCARLETTOS_SDK_H
