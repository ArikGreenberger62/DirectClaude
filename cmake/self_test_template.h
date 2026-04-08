/* self_test.h — copy to Core/Inc/self_test.h in each project */
#ifndef SELF_TEST_H
#define SELF_TEST_H

#ifdef SELF_TEST
void SelfTest_Run(void);
#else
/* No-op when SELF_TEST is not defined */
static inline void SelfTest_Run(void) {}
#endif

#endif /* SELF_TEST_H */
