#include "can-lite/categories/firmware_upgrade/test_doubles/TestUpgradePack.hpp"
#include "infra/util/ByteRange.hpp"
#include <algorithm>
#include <array>

namespace services::test
{
    namespace
    {
        template<std::size_t N>
        std::array<char, N> Text(infra::BoundedConstString text)
        {
            std::array<char, N> result{};
            std::copy_n(text.begin(), std::min(text.size(), N), result.begin());
            return result;
        }

        template<class T>
        void Append(TestUpgradePack& bytes, const T& value)
        {
            const auto range = infra::MakeByteRange(value);
            bytes.insert(bytes.end(), range.begin(), range.end());
        }

        application::StagedPackHeader MakeTestPackHeader(const TestPackDescription& description)
        {
            application::StagedPackHeader header{};
            header.prologue = application::UpgradePackHeaderPrologue{ application::UpgradePackStatus::readyToDeploy, std::array<uint8_t, 3>{ 'U', 'P', 'H' }, 0, 0, 2, testPackSignatureLength };
            header.epilogue.headerVersion = 1;
            header.epilogue.headerLength = sizeof(header.prologue) + testPackSignatureLength + sizeof(header.epilogue);
            header.epilogue.numberOfImages = 1;
            header.epilogue.productName = Text<64>(description.productName);
            header.epilogue.productVersion = Text<64>(description.productVersion);
            header.epilogue.componentName = Text<64>("component");
            header.image.lengthOfHeaderAndImage = sizeof(header.image) + testPackAesCounterSize + sizeof(application::ImageHeaderEpilogue) + description.imageSize;
            header.image.targetName = Text<8>(description.target);
            header.image.encryptionAndMacMethod = 1;
            return header;
        }
    }

    TestUpgradePack MakeTestPack(const TestPackDescription& description)
    {
        const auto header = MakeTestPackHeader(description);
        TestUpgradePack pack;
        Append(pack, header.prologue);
        pack.insert(pack.end(), testPackSignatureLength, 0x5a);
        Append(pack, header.epilogue);
        Append(pack, header.image);
        for (uint32_t i = 0; i != header.image.lengthOfHeaderAndImage - sizeof(header.image); ++i)
            pack.push_back(static_cast<uint8_t>(i * 7 + 3));
        return pack;
    }
}
