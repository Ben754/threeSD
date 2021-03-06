// Copyright 2019 threeSD Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include "common/common_types.h"
#include "common/thread.h"
#include "core/key/key.h"

namespace Core {

/// (current_size, total_size)
using ProgressCallback = std::function<void(std::size_t, std::size_t)>;

/**
 * Helper that reads, decrypts and writes data. This uses three threads to process the data
 * and call progress callbacks occasionally.
 *
 * While this is a template, it really should only be used with IOFile and SDMCFile.
 */
template <typename In = FileUtil::IOFile, typename Out = FileUtil::IOFile>
class QuickDecryptor {
public:
    /**
     * Initializes the decryptor.
     * @param root_folder Path to the "Nintendo 3DS/<ID0>/<ID1>" folder.
     */
    explicit QuickDecryptor();
    ~QuickDecryptor();

    /**
     * Decrypts and writes a file.
     *
     * @param source Source file
     * @param size Size to read, decrypt and write
     * @param destination Destination file
     * @param callback Progress callback
     * @param decrypt Whether to perform decryption or not
     * @param key AES Key for decryption
     * @param ctr AES CTR for decryption
     * @param aes_seek_pos The position to seek to for decryption.
     */
    bool DecryptAndWriteFile(std::shared_ptr<In> source, std::size_t size,
                             std::shared_ptr<Out> destination,
                             const ProgressCallback& callback = [](std::size_t, std::size_t) {},
                             bool decrypt = false, Core::Key::AESKey key = {},
                             Core::Key::AESKey ctr = {}, std::size_t aes_seek_pos = 0);

    void DataReadLoop();
    void DataDecryptLoop();
    void DataWriteLoop();

    void Abort();

    /// Reset the imported_size counter for this content and set a new total_size.
    void Reset(std::size_t total_size);

private:
    static constexpr std::size_t BufferSize = 16 * 1024; // 16 KB

    std::shared_ptr<In> source;
    std::shared_ptr<Out> destination;
    bool decrypt{};
    Core::Key::AESKey key;
    Core::Key::AESKey ctr;
    std::size_t aes_seek_pos;

    // Total size of this content, may consist of multiple files
    std::size_t total_size{};
    // Total size of the current file to process
    std::size_t current_total_size{};
    // Total imported size for this content
    std::size_t imported_size{};

    std::array<std::array<u8, BufferSize>, 3> buffers;
    std::array<Common::Event, 3> data_read_event;
    std::array<Common::Event, 3> data_decrypted_event;
    std::array<Common::Event, 3> data_written_event;

    std::unique_ptr<std::thread> read_thread;
    std::unique_ptr<std::thread> decrypt_thread;
    std::unique_ptr<std::thread> write_thread;

    ProgressCallback callback;

    Common::Event completion_event;
    bool is_good{true};
    std::atomic_bool is_running{false};
};

} // namespace Core
