#define NEUTRAL_ACTUATOR_POS 0.5 // steps/mm/etc where float is neutrally buoyant
#define ACTUATOR_MIN 0           // mechanical minimum
#define ACTUATOR_MAX 1           // mechanical maximum
#define BOUND 1000.0            // arbitrary integral windup bound
typedef struct
{
    double kp;
    double ki;
    double kd;
    double dt;
    double integral;
    double prev_error;
    double output_min;
    double output_max;
} PIDController;

void pid_init(PIDController *pid, double kp, double ki, double kd, double dt, double output_min, double output_max)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->dt = dt;
    pid->integral = 0.0;
    pid->prev_error = 0.0;
    pid->output_min = output_min;
    pid->output_max = output_max;
}

void pid_update(PIDController *pid, double error, double *output)
{
    // Proportional term
    double P = pid->kp * error;

    // Integral term
    pid->integral += error * pid->dt;

    // Simple anti-windup: clamp the internal integral sum
    if (pid->integral > BOUND)
    {
        pid->integral = BOUND;
    }
    if (pid->integral < -BOUND)
    {
        pid->integral = -BOUND;
    }
        
    double I = pid->ki * pid->integral;

    // Derivative term
    double derivative = (error - pid->prev_error) / pid->dt;
    double D = pid->kd * derivative;

    // Compute total output
    double raw_output = P + I + D;

    // Clamp output to min/max
    if (raw_output > pid->output_max)
    {
        raw_output = pid->output_max;
    }
    else if (raw_output < pid->output_min)
    {
        raw_output = pid->output_min;
    }

    // Update previous error
    pid->prev_error = error;

    // Set output
    *output = raw_output;
}

void pid_reset(PIDController *pid)
{
    pid->integral = 0.0;
    pid->prev_error = 0.0;
}
