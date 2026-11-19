// vk_screenshot.h -- "screenshot"/"screenshotJPEG" console commands.

#ifndef VK_SCREENSHOT_H_
#define VK_SCREENSHOT_H_

void vk_screenshot_init( void );
void vk_screenshot_shutdown( void );

// Call once per frame, after vk_end_frame() has presented. Captures and writes a pending request, if any.
void vk_screenshot_process_pending( void );

#endif // VK_SCREENSHOT_H_
