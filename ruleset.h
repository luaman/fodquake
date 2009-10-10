
const char *Ruleset_GetName(void);
qboolean Ruleset_AllowRJScripts(void);
qboolean Ruleset_AllowTimeRefresh(void);
qboolean Ruleset_AllowPacketCmd(void);
qboolean Ruleset_ValidateCvarChange(const cvar_t *cvar, const char *newstringvalue, float newfloatvalue);
qboolean Ruleset_AllowFTrigger(const char *triggername);
qboolean Ruleset_AllowMsgTriggers(void);

void Ruleset_CvarInit(void);

