#ifndef TG_AI_ALERT_THRESHOLDS_H
#define TG_AI_ALERT_THRESHOLDS_H
/* R11 C development_selection.json, INT8 all observed local candidates.
 * parity: R8 margin + min(R9 anchor margin,0) > 0.9375.
 * stop: C margin + min(R9 anchor margin,0) > 4.8125.
 * Both are signed Q11 residuals, NOT probabilities or R5 Q8 thresholds. */
#define TG_AI_ALERT_MODEL_ID 11U
#define TG_AI_PARITY_CUT_Q11 1920
#define TG_AI_STOP_CUT_Q11 9856
#endif
