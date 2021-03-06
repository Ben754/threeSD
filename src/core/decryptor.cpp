// Copyright 2019 threeSD Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <vector>
#include <cryptopp/aes.h>
#include <cryptopp/files.h>
#include <cryptopp/filters.h>
#include <cryptopp/modes.h>
#include <cryptopp/sha.h>
#include "common/assert.h"
#include "common/file_util.h"
#include "common/string_util.h"
#include "core/decryptor.h"
#include "core/key/key.h"

namespace Core {

SDMCDecryptor::SDMCDecryptor(const std::string& root_folder_) : root_folder(root_folder_) {

    ASSERT_MSG(Key::IsNormalKeyAvailable(Key::SDKey),
               "SD Key must be available in order to decrypt");

    if (root_folder.back() == '/' || root_folder.back() == '\\') {
        // Remove '/' or '\' character at the end as we will add them back when combining path
        root_folder.erase(root_folder.size() - 1);
    }
}

SDMCDecryptor::~SDMCDecryptor() = default;

namespace {
std::array<u8, 16> GetFileCTR(const std::string& path) {
    auto path_utf16 = Common::UTF8ToUTF16(path);
    std::vector<u8> path_data(path_utf16.size() * 2 + 2, 0); // Add the '\0' character
    std::memcpy(path_data.data(), path_utf16.data(), path_utf16.size() * 2);

    CryptoPP::SHA256 sha;
    std::array<u8, CryptoPP::SHA256::DIGESTSIZE> hash;
    sha.CalculateDigest(hash.data(), path_data.data(), path_data.size());

    std::array<u8, 16> ctr;
    for (int i = 0; i < 16; i++) {
        ctr[i] = hash[i] ^ hash[16 + i];
    }
    return ctr;
}
} // namespace

void SDMCDecryptor::Reset(std::size_t total_size) {
    quick_decryptor.Reset(total_size);
}

bool SDMCDecryptor::DecryptAndWriteFile(const std::string& source, const std::string& destination,
                                        const ProgressCallback& callback) {
    if (!FileUtil::CreateFullPath(destination)) {
        LOG_ERROR(Core, "Could not create path {}", destination);
        return false;
    }

    auto source_file = std::make_shared<FileUtil::IOFile>(root_folder + source, "rb");
    auto size = source_file->GetSize();
    auto destination_file = std::make_shared<FileUtil::IOFile>(destination, "wb");
    auto key = Key::GetNormalKey(Key::SDKey);
    auto ctr = GetFileCTR(source);
    return quick_decryptor.DecryptAndWriteFile(std::move(source_file), size,
                                               std::move(destination_file), callback, true,
                                               std::move(key), std::move(ctr));
}

void SDMCDecryptor::Abort() {
    quick_decryptor.Abort();
}

std::vector<u8> SDMCDecryptor::DecryptFile(const std::string& source) const {
    auto ctr = GetFileCTR(source);
    auto key = Key::GetNormalKey(Key::SDKey);
    CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption aes;
    aes.SetKeyWithIV(key.data(), key.size(), ctr.data());

    FileUtil::IOFile file(root_folder + source, "rb");
    if (!file) {
        LOG_ERROR(Core, "Could not open {}", root_folder + source);
        return {};
    }

    auto size = file.GetSize();

    std::vector<u8> encrypted_data(size);
    if (file.ReadBytes(encrypted_data.data(), size) != size) {
        LOG_ERROR(Core, "Could not read file {}", root_folder + source);
        return {};
    }

    std::vector<u8> data(size);
    aes.ProcessData(data.data(), encrypted_data.data(), encrypted_data.size());
    return data;
}

SDMCFile::SDMCFile() = default;

SDMCFile::SDMCFile(std::string root_folder, const std::string& filename, const char openmode[],
                   int flags) {
    if (root_folder.back() == '/' || root_folder.back() == '\\') {
        // Remove '/' or '\' character at the end as we will add them back when combining path
        root_folder.erase(root_folder.size() - 1);
    }

    original_ctr = GetFileCTR(filename);
    key = Key::GetNormalKey(Key::SDKey);
    // aes.SetKeyWithIV(key.data(), key.size(), original_ctr.data());

    Open(root_folder + filename, openmode, flags);
}

SDMCFile::~SDMCFile() {
    Close();
}

SDMCFile::SDMCFile(SDMCFile&& other) {
    Swap(other);
}

SDMCFile& SDMCFile::operator=(SDMCFile&& other) {
    Swap(other);
    return *this;
}

void SDMCFile::Swap(SDMCFile& other) {
    file.Swap(other.file);
    std::swap(original_ctr, other.original_ctr);
    std::swap(key, other.key);
}

bool SDMCFile::Open(const std::string& filename, const char openmode[], int flags) {
    return file.Open(filename, openmode, flags);
}

bool SDMCFile::Close() {
    return file.Close();
}

u64 SDMCFile::GetSize() const {
    return file.GetSize();
}

bool SDMCFile::Seek(s64 off, int origin) {
    return file.Seek(off, origin);
}

u64 SDMCFile::Tell() const {
    return file.Tell();
}

void SDMCFile::DecryptData(u8* data, std::size_t size) {
    CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption aes;
    aes.SetKeyWithIV(key.data(), key.size(), original_ctr.data());
    aes.Seek(Tell() - size);
    aes.ProcessData(data, data, size);
}

} // namespace Core
