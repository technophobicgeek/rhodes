#ifndef RHO_CLASS_FACTORY_b15393f788404746b0b59f04829e14ea
#define RHO_CLASS_FACTORY_b15393f788404746b0b59f04829e14ea

#include <common/IRhoClassFactory.h>
#include <net/INetRequest.h>
#include <net/ssl.h>
#include <common/IRhoThreadImpl.h>
#include <common/IRhoCrypt.h>

namespace rho
{
namespace common
{

class CRhoClassFactory : public IRhoClassFactory
{
public:
    net::INetRequestImpl* createNetRequestImpl();
    IRhoThreadImpl *createThreadImpl();
    net::ISSL *createSSLEngine();
    IRhoCrypt* createRhoCrypt();
};

} // namespace common
} // namespace rho

#endif // RHO_CLASS_FACTORY_b15393f788404746b0b59f04829e14ea

