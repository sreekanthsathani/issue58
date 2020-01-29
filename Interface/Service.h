#include "CustomClient.h"
#include "Object.h"

class IService : public IObject
{
public:
	virtual ICustomClient* createClient()=0;
	virtual void destroyClient( ICustomClient * pClient)=0;
};
