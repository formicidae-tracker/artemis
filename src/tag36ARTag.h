#ifndef _TAG36ARTAG
#define _TAG36ARTAG

#ifdef __cplusplus
extern "C" {
#endif

apriltag_family_t *tag36ARTag_create();
void tag36ARTag_destroy(apriltag_family_t *tf);

uint16_t tag36ARTag_legacy_ID(uint16_t ID);

#ifdef __cplusplus
}
#endif

#endif
