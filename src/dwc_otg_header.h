#ifndef DWC_OTG_DEVICE_SELECT_H
#define DWC_OTG_DEVICE_SELECT_H

#if defined(STM32F722xx) \
 || defined(STM32F723xx) \
 || defined(STM32F732xx) \
 || defined(STM32F733xx) \
 || defined(STM32F756xx) \
 || defined(STM32F746xx) \
 || defined(STM32F745xx) \
 || defined(STM32F765xx) \
 || defined(STM32F767xx) \
 || defined(STM32F769xx) \
 || defined(STM32F777xx) \
 || defined(STM32F779xx)
#include <stm32f7xx.h>
#elif defined (STM32F405xx) \
 || defined (STM32F415xx) \
 || defined (STM32F407xx) \
 || defined (STM32F417xx) \
 || defined (STM32F427xx) \
 || defined (STM32F437xx) \
 || defined (STM32F429xx) \
 || defined (STM32F439xx) \
 || defined (STM32F401xC) \
 || defined (STM32F401xE) \
 || defined (STM32F410Tx) \
 || defined (STM32F410Cx) \
 || defined (STM32F410Rx) \
 || defined (STM32F411xE) \
 || defined (STM32F446xx) \
 || defined (STM32F469xx) \
 || defined (STM32F479xx) \
 || defined (STM32F412Cx) \
 || defined (STM32F412Rx) \
 || defined (STM32F412Vx) \
 || defined (STM32F412Zx) \
 || defined (STM32F413xx) \
 || defined (STM32F423xx)
#include <stm32f4xx.h>
#else
#error No device #define
#endif

#endif // DWC_OTG_DEVICE_SELECT_H
