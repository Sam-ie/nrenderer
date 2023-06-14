#include "server/Server.hpp"
#include "scene/Scene.hpp"
#include "component/RenderComponent.hpp"
#include "Camera.hpp"

#include "PhotonMapping.hpp"

using namespace std;
using namespace NRenderer;

namespace PhotonMapping
{
    class Adapter : public RenderComponent
    {
        /* Same as simple path tracing */
        void render(SharedScene spScene) {
            PhotonMappingRenderer renderer{spScene};
            auto renderResult = renderer.render();
            auto [ pixels, width, height ]  = renderResult;
            getServer().screen.set(pixels, width, height);
            renderer.release(renderResult);
        }
    };
}

const static string description = 
    "The Photon Mapping. "
    "\nPlease use scene file : photon_mapping.scn";

REGISTER_RENDERER(PhotonMapping, description, PhotonMapping::Adapter);