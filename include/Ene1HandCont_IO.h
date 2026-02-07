#ifndef ENE1_HANDCONT_IO_H
#define ENE1_HANDCONT_IO_H

#include "ADInput.h"
#include "DigitalInput.h"

/**
 * @file Ene1HandCont_IO.h
 * @brief IOインスタンスの外部参照用定義
 */

// グローバルIOインスタンス
extern ADInputChannel adAccel;
extern ADInputChannel adBrake;
extern DigitalInputChannel diKeyUp;
extern DigitalInputChannel diKeyDown;

#endif // ENE1_HANDCONT_IO_H