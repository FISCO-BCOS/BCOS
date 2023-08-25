%{
#include "bcos-crypto/interfaces/crypto/CryptoSuite.h"
#include "bcos-crypto/interfaces/crypto/Hash.h"
#include "bcos-crypto/hash/Keccak256.h"
#include "bcos-crypto/hash/SM3.h"
#include "bcos-crypto/signature/secp256k1/Secp256k1Crypto.h"
#include "bcos-crypto/signature/fastsm2/FastSM2Crypto.h"
#include "bcos-crypto/encrypt/AESCrypto.h"
#include "bcos-crypto/encrypt/SM4Crypto.h"

using namespace bcos::crypto;

inline std::shared_ptr<bcos::crypto::CryptoSuite> newCryptoSuite(bool sm) {
    if (sm) {
        return std::make_shared<CryptoSuite>(std::make_shared<SM3>(), std::make_shared<FastSM2Crypto>(), std::make_shared<SM4Crypto>());
    }
    return std::make_shared<CryptoSuite>(std::make_shared<Keccak256>(), std::make_shared<Secp256k1Crypto>(), std::make_shared<AESCrypto>());
}
inline const bcos::crypto::KeyPairInterface& pointerToReference(const bcos::crypto::KeyPairInterface::UniquePtr& ptr) {
    return *ptr;
}
%}

%include <stdint.i>
%include <std_shared_ptr.i>
%shared_ptr(bcos::crypto::CryptoSuite)

%include "../bcos-crypto/bcos-crypto/interfaces/crypto/KeyPairInterface.h"
%include "../bcos-crypto/bcos-crypto/interfaces/crypto/CryptoSuite.h"
%include "../bcos-crypto/bcos-crypto/interfaces/crypto/Hash.h"
%include "../bcos-crypto/bcos-crypto/interfaces/crypto/Signature.h"
%include "../bcos-crypto/bcos-crypto/interfaces/crypto/SymmetricEncryption.h"
%include "../bcos-crypto/bcos-crypto/signature/secp256k1/Secp256k1Crypto.h"
%include "../bcos-crypto/bcos-crypto/signature/fastsm2/FastSM2Crypto.h"
%include "../bcos-crypto/bcos-crypto/encrypt/AESCrypto.h"
%include "../bcos-crypto/bcos-crypto/encrypt/SM4Crypto.h"
%include "../bcos-crypto/bcos-crypto/interfaces/crypto/CommonType.h"

inline std::shared_ptr<bcos::crypto::CryptoSuite> newCryptoSuite(bool sm);
inline const bcos::crypto::KeyPairInterface& pointerToReference(const bcos::crypto::KeyPairInterface::UniquePtr& ptr);

