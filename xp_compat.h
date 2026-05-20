#ifndef XP_COMPAT_H
#define XP_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

// Инициализация совместимости с Windows XP
// ВАЖНО: Вызвать ДО инициализации cURL!
void InitXPCompat(void);

#ifdef __cplusplus
}
#endif

#endif // XP_COMPAT_H
