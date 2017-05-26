#define LINUX

#include "erl_nif.h"
#include "digital_signature_lib.c"

static ERL_NIF_TERM
ProcessPKCS7Data(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  void* libHandler;

  char libPath[50] = "";
  char* configuredPath = getenv("UACRYPTO_LIB_PATH");
  char* homePath = getenv("HOME");
  char* libName = "/libUACryptoQ.so";

  if (configuredPath != NULL) {
    strcat(libPath, configuredPath);
  }
  else {
    strcat(libPath, homePath);
    strcat(libPath, libName);
  }

  libHandler = dlopen(libPath, RTLD_LAZY);
  if (!libHandler) {
    char* error = dlerror();
    ErlNifBinary errorBin = {strlen(error), error};
    ERL_NIF_TERM errorTerm = enif_make_binary(env, &errorBin);
    return enif_make_tuple2(env, enif_make_atom(env, "error"), errorTerm);
  }

  unsigned int p7DataLength;
  enif_get_list_length(env, argv[0], &p7DataLength);
  if (p7DataLength == 0) {
    ERL_NIF_TERM errorTerm = enif_make_string(env, "pkcs7 data is empty", ERL_NIF_LATIN1);
    return enif_make_tuple2(env, enif_make_atom(env, "error"), errorTerm);
  }
  char* p7Data = malloc(p7DataLength);
  enif_get_string(env, argv[0], p7Data, p7DataLength + 1, ERL_NIF_LATIN1);
  UAC_BLOB signedData = { p7Data, p7DataLength };

  struct Certs certs;

  ERL_NIF_TERM generalCerts;
  enif_get_map_value(env, argv[1], enif_make_atom(env, "general"), &generalCerts);
  unsigned int generalCertsLength;
  enif_get_list_length(env, generalCerts, &generalCertsLength);
  for (int i = 0; i < generalCertsLength; i++) {
    ERL_NIF_TERM firstItem;
    ERL_NIF_TERM rest;
    enif_get_list_cell(env, generalCerts, &firstItem, &rest);

    ERL_NIF_TERM rootCertTerm;
    enif_get_map_value(env, firstItem, enif_make_atom(env, "root"), &rootCertTerm);
    unsigned int rootCertDataLength;
    enif_get_list_length(env, rootCertTerm, &rootCertDataLength);
    char* rootCertData = malloc(rootCertDataLength);
    enif_get_string(env, rootCertTerm, rootCertData, rootCertDataLength + 1, ERL_NIF_LATIN1);
    UAC_BLOB rootCert = { rootCertData, rootCertDataLength };

    ERL_NIF_TERM ocspCertTerm;
    enif_get_map_value(env, firstItem, enif_make_atom(env, "ocsp"), &ocspCertTerm);
    unsigned int ocspCertDataLength;
    enif_get_list_length(env, ocspCertTerm, &ocspCertDataLength);
    char* ocspCertData = malloc(ocspCertDataLength);
    enif_get_string(env, ocspCertTerm, ocspCertData, ocspCertDataLength + 1, ERL_NIF_LATIN1);
    UAC_BLOB ocspCert = { ocspCertData, ocspCertDataLength };

    certs.general[i].root = rootCert;
    certs.general[i].ocsp = ocspCert;
    generalCerts = rest;
  }

  UAC_BLOB emptyBlob = {};
  certs.general[generalCertsLength].root = emptyBlob;
  certs.general[generalCertsLength].ocsp = emptyBlob;

  ERL_NIF_TERM tspCerts;
  enif_get_map_value(env, argv[1], enif_make_atom(env, "tsp"), &tspCerts);
  unsigned int tspCertsLength;
  enif_get_list_length(env, tspCerts, &tspCertsLength);
  for (int i = 0; i < tspCertsLength; i++) {
    ERL_NIF_TERM firstItem;
    ERL_NIF_TERM rest;
    enif_get_list_cell(env, tspCerts, &firstItem, &rest);
    unsigned int tspCertDataLength;
    enif_get_list_length(env, firstItem, &tspCertDataLength);
    char* tspCertData = malloc(tspCertDataLength);
    enif_get_string(env, firstItem, tspCertData, tspCertDataLength + 1, ERL_NIF_LATIN1);
    UAC_BLOB tspCert = { tspCertData, tspCertDataLength };

    certs.tsp[i] = tspCert;
    tspCerts = rest;
  }

  certs.tsp[tspCertsLength] = emptyBlob;

  UAC_BLOB dataBlob = {enif_alloc(signedData.dataLen), signedData.dataLen};

  UAC_SIGNED_DATA_INFO signedDataInfo = {};
  DWORD loadSignedDataResult = LoadSignedData(libHandler, signedData, &dataBlob, &signedDataInfo);

  PUAC_SUBJECT_INFO subjectInfo = malloc(signedData.dataLen);
  bool checkResult = Check(libHandler, signedData, signedDataInfo, subjectInfo, certs);
  dlclose(libHandler);

  ERL_NIF_TERM signer = enif_make_new_map(env);
  char* commonName = subjectInfo->commonName;
  ErlNifBinary commonNameBin = {strlen(commonName), commonName};
  ERL_NIF_TERM commonNameTerm = enif_make_binary(env, &commonNameBin);
  enif_make_map_put(env, signer, enif_make_atom(env, "common_name"), commonNameTerm, &signer);
  char* countryName = subjectInfo->countryName;
  ErlNifBinary countryNameBin = {strlen(countryName), countryName};
  ERL_NIF_TERM countryNameTerm = enif_make_binary(env, &countryNameBin);
  enif_make_map_put(env, signer, enif_make_atom(env, "country_name"), countryNameTerm, &signer);
  char* surname = subjectInfo->surname;
  ErlNifBinary surnameBin = {strlen(surname), surname};
  ERL_NIF_TERM surnameTerm = enif_make_binary(env, &surnameBin);
  enif_make_map_put(env, signer, enif_make_atom(env, "surname"), surnameTerm, &signer);
  char* givenName = subjectInfo->givenName;
  ErlNifBinary givenNameBin = {strlen(givenName), givenName};
  ERL_NIF_TERM givenNameTerm = enif_make_binary(env, &givenNameBin);
  enif_make_map_put(env, signer, enif_make_atom(env, "given_name"), givenNameTerm, &signer);
  char* organizationName = subjectInfo->organizationName;
  ErlNifBinary organizationNameBin = {strlen(organizationName), organizationName};
  ERL_NIF_TERM organizationNameTerm = enif_make_binary(env, &organizationNameBin);
  enif_make_map_put(env, signer, enif_make_atom(env, "organization_name"), organizationNameTerm, &signer);
  char* stateOrProvinceName = subjectInfo->stateOrProvinceName;
  ErlNifBinary stateOrProvinceNameBin = {strlen(stateOrProvinceName), stateOrProvinceName};
  ERL_NIF_TERM stateOrProvinceNameTerm = enif_make_binary(env, &stateOrProvinceNameBin);
  enif_make_map_put(env, signer, enif_make_atom(env, "state_or_province_name"), stateOrProvinceNameTerm, &signer);
  char* localityName = subjectInfo->localityName;
  ErlNifBinary localityNameBin = {strlen(localityName), localityName};
  ERL_NIF_TERM localityNameTerm = enif_make_binary(env, &localityNameBin);
  enif_make_map_put(env, signer, enif_make_atom(env, "locality_name"), localityNameTerm, &signer);
  char* organizationalUnitName = subjectInfo->organizationalUnitName;
  ErlNifBinary organizationalUnitNameBin = {strlen(organizationalUnitName), organizationalUnitName};
  ERL_NIF_TERM organizationalUnitNameTerm = enif_make_binary(env, &organizationalUnitNameBin);
  enif_make_map_put(env, signer, enif_make_atom(env, "organizational_unit_name"), organizationalUnitNameTerm, &signer);
  char* title = subjectInfo->title;
  ErlNifBinary titleBin = {strlen(title), title};
  ERL_NIF_TERM titleTerm = enif_make_binary(env, &titleBin);
  enif_make_map_put(env, signer, enif_make_atom(env, "title"), titleTerm, &signer);
  char* edrpou = subjectInfo->edrpou;
  ErlNifBinary edrpouBin = {strlen(edrpou), edrpou};
  ERL_NIF_TERM edrpouTerm = enif_make_binary(env, &edrpouBin);
  enif_make_map_put(env, signer, enif_make_atom(env, "edrpou"), edrpouTerm, &signer);
  char* drfo = subjectInfo->drfo;
  ErlNifBinary drfoBin = {strlen(drfo), drfo};
  ERL_NIF_TERM drfoTerm = enif_make_binary(env, &drfoBin);
  enif_make_map_put(env, signer, enif_make_atom(env, "drfo"), drfoTerm, &signer);

  char* data = dataBlob.data;
  ErlNifBinary dataBin = {strlen(data), data};
  ERL_NIF_TERM content = enif_make_binary(env, &dataBin);
  ERL_NIF_TERM result = enif_make_new_map(env);
  enif_make_map_put(env, result, enif_make_atom(env, "content"), content, &result);
  enif_make_map_put(env, result, enif_make_atom(env, "signer"), signer, &result);
  enif_make_map_put(env, result, enif_make_atom(env, "is_valid"), enif_make_int(env, checkResult), &result);
  return enif_make_tuple2(env, enif_make_atom(env, "ok"), result);
}

static ErlNifFunc nif_funcs[] = {
  {"processPKCS7Data", 2, ProcessPKCS7Data}
};

ERL_NIF_INIT(digital_signature_lib, nif_funcs, NULL, NULL, NULL, NULL);
