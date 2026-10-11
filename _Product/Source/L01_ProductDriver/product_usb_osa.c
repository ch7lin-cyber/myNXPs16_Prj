#include "fsl_os_abstraction.h"
#include <string.h>

/* CDC invokes only mutex creation/destruction in this bare-metal SDK.
 * Transfers are serialized with OSA_EnterCritical/ExitCritical in the driver;
 * no task scheduler, OSA timer, or additional SysTick handler is installed.
 * Keep these lifecycle hooks consistent with the SDK bare-metal implementation.
 */
osa_status_t OSA_MutexCreate(osa_mutex_handle_t handle)
{
    if (handle == NULL)
    {
        return KOSA_StatusError;
    }
    (void)memset(handle, 0, OSA_MUTEX_HANDLE_SIZE);
    return KOSA_StatusSuccess;
}

osa_status_t OSA_MutexDestroy(osa_mutex_handle_t handle)
{
    if (handle == NULL)
    {
        return KOSA_StatusError;
    }
    (void)memset(handle, 0, OSA_MUTEX_HANDLE_SIZE);
    return KOSA_StatusSuccess;
}
