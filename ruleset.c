#include <string.h>

#include "qtypes.h"
#include "cvar.h"
#include "ruleset.h"

struct ruleset
{
	const char *name;
	void (*Init)(void);
	qboolean (*AllowRJScripts)(void);
	qboolean (*AllowTimeRefresh)(void);
	qboolean (*AllowPacketCmd)(void);
	qboolean (*ValidateCvarChange)(const cvar_t *cvar, const char *newstringvalue, float newfloatvalue);
	qboolean (*AllowFTrigger)(const char *triggername);
	qboolean (*AllowMsgTriggers)(void);
};

/* --- Default ruleset */

static void Ruleset_Default_Init()
{
}

static qboolean Ruleset_Default_AllowRJScripts()
{
	return true;
}

static qboolean Ruleset_Default_AllowTimeRefresh()
{
	return true;
}

static qboolean Ruleset_Default_AllowPacketCmd()
{
	return true;
}

static qboolean Ruleset_Default_ValidateCvarChange(const cvar_t *cvar, const char *newstringvalue, float newfloatvalue)
{
	return true;
}

static qboolean Ruleset_Default_AllowFTrigger(const char *triggername)
{
	return true;
}

static qboolean Ruleset_Default_AllowMsgTriggers()
{
	return true;
}

static const struct ruleset ruleset_default =
{
	"default",
	Ruleset_Default_Init,
	Ruleset_Default_AllowRJScripts,
	Ruleset_Default_AllowTimeRefresh,
	Ruleset_Default_AllowPacketCmd,
	Ruleset_Default_ValidateCvarChange,
	Ruleset_Default_AllowFTrigger,
	Ruleset_Default_AllowMsgTriggers,
};

/* --- EQL ruleset */

static void Ruleset_EQL_Init()
{
}

static qboolean Ruleset_EQL_AllowRJScripts()
{
	return false;
}

static qboolean Ruleset_EQL_AllowTimeRefresh()
{
	return false;
}

static qboolean Ruleset_EQL_AllowPacketCmd()
{
	return false;
}

static qboolean Ruleset_EQL_ValidateCvarChange(const cvar_t *cvar, const char *newstringvalue, float newfloatvalue)
{
	if (strcmp(cvar->name, "r_aliasstats") == 0
	 || strcmp(cvar->name, "cl_imitate_client") == 0
	 || strcmp(cvar->name, "cl_imitate_os") == 0)
	{
		return false;
	}

	return true;
}

static qboolean Ruleset_EQL_AllowFTrigger(const char *triggername)
{
	if (strcmp(triggername, "f_took") == 0
	 || strcmp(triggername, "f_respawn") == 0
	 || strcmp(triggername, "f_death") == 0
	 || strcmp(triggername, "f_flagdeath") == 0)
	{
		return false;
	}

	return true;
}

static qboolean Ruleset_EQL_AllowMsgTriggers()
{
	return false;
}

static const struct ruleset ruleset_eql =
{
	"eql",
	Ruleset_EQL_Init,
	Ruleset_EQL_AllowRJScripts,
	Ruleset_EQL_AllowTimeRefresh,
	Ruleset_EQL_AllowPacketCmd,
	Ruleset_EQL_ValidateCvarChange,
	Ruleset_EQL_AllowFTrigger,
	Ruleset_EQL_AllowMsgTriggers,
};

/* --- */

static const struct ruleset *ruleset = &ruleset_default;

const char *Ruleset_GetName()
{
	return ruleset->name;
}

qboolean Ruleset_AllowRJScripts()
{
	return ruleset->AllowRJScripts();
}

qboolean Ruleset_AllowTimeRefresh()
{
	return ruleset->AllowTimeRefresh();
}

qboolean Ruleset_AllowPacketCmd()
{
	return ruleset->AllowPacketCmd();
}

qboolean Ruleset_ValidateCvarChange(const cvar_t *cvar, const char *newstringvalue, float newfloatvalue)
{
	return ruleset->ValidateCvarChange(cvar, newstringvalue, newfloatvalue);
}

qboolean Ruleset_AllowFTrigger(const char *triggername)
{
	return ruleset->AllowFTrigger(triggername);
}

qboolean Ruleset_AllowMsgTriggers()
{
	return ruleset->AllowMsgTriggers();
}

/* --- */

static cvar_t cl_ruleset = { "cl_ruleset", "default" };

void Ruleset_CvarInit()
{
	Cvar_Register(&cl_ruleset);
}

