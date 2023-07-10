#ifndef JPEG_LOADER_H
#define JPEG_LOADER_H

#include "imageloaderprovider.h"

namespace khtmlImLoad {

class ImageLoader;

class JPEGLoaderProvider: public ImageLoaderProvider
{
public:
    Type type() override;

    ImageLoader* loaderFor(const QByteArray& prefix) override;
};

}

#endif
// kate: indent-width 4; replace-tabs on; tab-width 4; space-indent on;
