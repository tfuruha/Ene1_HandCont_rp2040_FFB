/**
 * @file control.h
 * @brief 物理エフェクト計算およびPID制御クラスの定義
 */

#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>

/**
 * @brief 物理エフェクト計算クラス
 */
class PhysicalEffect {
public:
  PhysicalEffect(float k_f, float k_s, float k_d, float k_i,
                 uint32_t period_us);
  void update(int16_t angle);
  int16_t getEffect() const;

private:
  float _k_friction;
  float _k_spring;
  float _k_damper;
  float _k_inertia;
  float _dt_sec;

  float _prev_angle;
  float _prev_velocity;
  int16_t _current_output;
};

/**
 * @brief PID制御クラス
 */
class PID {
public:
  PID(float kp, float ki, float kd, uint32_t period_us, int16_t target);
  void update(int16_t current_angle);
  int16_t getPID() const;
  void setTarget(int16_t target);

private:
  float _kp, _ki, _kd;
  float _dt_sec;
  int16_t _target;

  float _integral;
  float _prev_error;
  int16_t _current_output;
};

#endif // CONTROL_H
