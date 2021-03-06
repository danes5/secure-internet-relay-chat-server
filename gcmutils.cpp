#include "gcmutils.h"
#include <QDataStream>
#include <QDebug>

GcmUtils::GcmUtils() {}

QByteArray GcmUtils::encryptAndTag(QByteArray data) {
  unsigned char tag[16];
  quint64 length = data.length() + tag_len;

  unsigned char output[length + sizeof(quint64)];
  encryptDataGCM((const unsigned char *)data.constData(), length - tag_len,
                 &context, nullptr, 0, iv, iv_len, tag_len, tag,
                 output + tag_len + sizeof(quint64));
  QByteArray encrypted;
  QDataStream stream(&encrypted, QIODevice::ReadWrite);
  memcpy(output, (const char *)&length, sizeof(quint64));
  memcpy(output + sizeof(quint64), tag, tag_len);
  encrypted = QByteArray((const char *)output, length + sizeof(quint64));
  return encrypted;
}

QJsonDocument GcmUtils::decryptAndAuthorizeFull(QByteArray array) {
  const char *tagArray = array.constData();
  QByteArray body = array.mid(tag_len, -1);
  return decryptAndAuthorizeBody(body, tagArray);
}

QJsonDocument GcmUtils::decryptAndAuthorizeBody(QByteArray array,
                                                const char *tag) {
  unsigned char output[array.length()];
  int result = decryptDataGCM((const unsigned char *)array.constData(),
                              array.length(), &context, nullptr, 0, iv, iv_len,
                              tag_len, (const unsigned char *)tag, output);
  if (result != 0) {
    // this means an error during decryption
    qDebug() << "error occured during decryption" << result;
  }
  QByteArray decryptedData = QByteArray((const char *)output, array.length());

  QJsonDocument doc = QJsonDocument::fromBinaryData(decryptedData);
  return doc;
}

void GcmUtils::initialize() { mbedtls_gcm_init(&context); }

int GcmUtils::generateGcmKey() {
  mbedtls_entropy_init(&entropy);

  mbedtls_ctr_drbg_init(&ctr_drbg);
  int ret;

  std::string pers = "aes generate key";

  if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                   (unsigned char *)pers.c_str(),
                                   pers.length())) != 0) {
    printf(" failed\n ! mbedtls_ctr_drbg_init returned -0x%04x\n", -ret);
    return ret;
  }

  if ((ret = mbedtls_ctr_drbg_random(&ctr_drbg, key, 32)) != 0) {
    printf(" failed\n ! mbedtls_ctr_drbg_random returned -0x%04x\n", -ret);
    return ret;
  }
  ret = mbedtls_gcm_setkey(&context, MBEDTLS_CIPHER_ID_AES, key, keyBits * 8);
  if (ret != 0) {
    qDebug() << "could not set key";
  }
  return ret;
}

bool GcmUtils::setKey(unsigned char *newKey) {
  memcpy(key, newKey, keyBits);
  int result =
      mbedtls_gcm_setkey(&context, MBEDTLS_CIPHER_ID_AES, newKey, keyBits * 8);
  if (result != 0) {
    printf(" failed\n ! mbedtls_ctr_drbg_random returned -0x%04x\n", -result);
    return false;
  }
  return true;
}

QString GcmUtils::getKey() {
  return QString::fromLatin1((const char *)key, keyBits);
}
