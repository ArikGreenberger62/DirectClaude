/* self_test.h — 01-blink startup self-test */
#ifndef SELF_TEST_H
#define SELF_TEST_H

#ifdef SELF_TEST
void SelfTest_Run(void);
#else
static inline void SelfTest_Run(void) {}
#endif

#endif /* SELF_TEST_H */
