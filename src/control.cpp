/**
 * @file control.cpp
 * @brief 物理エフェクト計算およびPID制御クラスの実装
 */

#include "control.h"

// --- PhysicalEffect Class Implementation ---

PhysicalEffect::PhysicalEffect(float k_f, float k_s, float k_d, float k_i,
                               uint32_t period_us)
    : _k_friction(k_f), _k_spring(k_s), _k_damper(k_d), _k_inertia(k_i),
      _prev_angle(0.0f), _prev_velocity(0.0f), _current_output(0) {
  _dt_sec = (float)period_us / 1000000.0f;
}

void PhysicalEffect::update(int16_t angle) {
  float f_angle = (float)angle;
  float velocity = (f_angle - _prev_angle) / _dt_sec;
  float acceleration = (velocity - _prev_velocity) / _dt_sec;

  // フリクション項: 定数 * sign(速度)
  float friction_val = 0.0f;
  if (velocity > 0.1f)
    friction_val = 1.0f;
  else if (velocity < -0.1f)
    friction_val = -1.0f;

  float term_friction = _k_friction * friction_val;
  float term_spring = _k_spring * f_angle;
  float term_damper = _k_damper * velocity;
  float term_inertia = _k_inertia * acceleration;

  // すべて負のフィードバック（反発力）として計算し、合計する
  float total_f =
      -1.0f * (term_friction + term_spring + term_damper + term_inertia);

  // 飽和処理が必要な場合はここで行う（int16_tの範囲に収める）
  if (total_f > 32767.0f)
    total_f = 32767.0f;
  if (total_f < -32767.0f)
    total_f = -32767.0f;

  _current_output = (int16_t)total_f;

  _prev_angle = f_angle;
  _prev_velocity = velocity;
}

int16_t PhysicalEffect::getEffect() const { return _current_output; }

// --- PID Class Implementation ---

PID::PID(float kp, float ki, float kd, uint32_t period_us, int16_t target)
    : _kp(kp), _ki(ki), _kd(kd), _target(target), _integral(0.0f),
      _prev_error(0.0f), _current_output(0) {
  _dt_sec = (float)period_us / 1000000.0f;
}

void PID::update(int16_t current_angle) {
  float error = (float)(_target - current_angle);

  // P項
  float p_term = _kp * error;

  // I項
  _integral += error * _dt_sec;
  float i_term = _ki * _integral;

  // D項
  float derivative = (error - _prev_error) / _dt_sec;
  float d_term = _kd * derivative;

  float total_pid = p_term + i_term + d_term;

  // 飽和処理
  if (total_pid > 32767.0f)
    total_pid = 32767.0f;
  if (total_pid < -32767.0f)
    total_pid = -32767.0f;

  _current_output = (int16_t)total_pid;
  _prev_error = error;
}

int16_t PID::getPID() const { return _current_output; }

void PID::setTarget(int16_t target) { _target = target; }
