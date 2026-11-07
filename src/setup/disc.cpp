#include "setup/disc.h"

#include "port/dol.h"

#include <nod.h>

namespace setup {

std::string hex(const std::uint8_t* data, size_t n) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; i++) {
        s += d[data[i] >> 4];
        s += d[data[i] & 15];
    }
    return s;
}

Disc::~Disc() {
    nod_free(part_);
    nod_free(disc_);
}

bool Disc::open(const std::string& utf8Path, std::string& error) {
    if (nod_disc_open(utf8Path.c_str(), nullptr, &disc_) != NOD_RESULT_OK) {
        const char* msg = nod_error_message();
        error = std::string("Could not read the disc image") + (msg ? std::string(": ") + msg : ".");
        return false;
    }
    if (nod_disc_open_partition_kind(disc_, NOD_PARTITION_KIND_DATA, nullptr, &part_) != NOD_RESULT_OK) {
        error = "The disc image has no game partition.";
        return false;
    }
    NodPartitionMeta meta{};
    if (nod_partition_meta(part_, &meta) != NOD_RESULT_OK || !meta.raw_dol.data) {
        error = "The disc image has no main.dol.";
        return false;
    }
    dol_.assign(meta.raw_dol.data, meta.raw_dol.data + meta.raw_dol.size);
    std::string id;
    int revision = -1;
    if (meta.raw_boot.data && meta.raw_boot.size >= 8) {
        id.assign(reinterpret_cast<const char*>(meta.raw_boot.data), 6);
        revision = meta.raw_boot.data[7];
    }
    if (id == "GLME01" || id == "GLMP01") {
        if (revision != 0) {
            error = "This Luigi's Mansion disc (" + id + ") revision " + std::to_string(revision) +
                    " is not supported. Supported: revision 0 of USA (GLME01) and Europe (GLMP01).";
            return false;
        }
        const port::dol::Version want = id == "GLME01" ? port::dol::Version::GLME01 : port::dol::Version::GLMP01;
        if (port::dol::identify(dol_.data(), dol_.size()) != want) {
            error = "The disc's main.dol changes code or section layout. Only data sections may change.";
            return false;
        }
        version_ = id == "GLME01" ? port::GameVersion::USA : port::GameVersion::PAL;
        return true;
    }
    if (id.rfind("GLM", 0) == 0)
        error = "This Luigi's Mansion disc (" + id + ") is not a supported release. Supported: USA (GLME01) and "
                "Europe (GLMP01), first printing.";
    else
        error = "This is not a Luigi's Mansion disc" + (id.empty() ? std::string(".") : " (game ID " + id + ").");
    return false;
}

std::string Disc::dolSha1() const {
    const auto d = port::dol::sha1(dol_.data(), dol_.size());
    return hex(d.data(), d.size());
}

bool Disc::readFile(const char* path, std::vector<std::uint8_t>& out) const {
    NodNodeKind kind{};
    uint32_t length = 0;
    const uint32_t index = nod_partition_find_file(part_, path, &kind, &length);
    if (index == NOD_FST_STOP) return false;
    NodHandle* file = nullptr;
    if (nod_partition_open_file(part_, index, &file) != NOD_RESULT_OK) return false;
    out.resize(length);
    size_t done = 0;
    while (done < length) {
        const int64_t n = nod_read(file, out.data() + done, length - done);
        if (n <= 0) break;
        done += size_t(n);
    }
    nod_free(file);
    return done == length;
}

bool probeDisc(const std::string& utf8Path, port::GameVersion& version, std::string& error) {
    Disc d;
    if (!d.open(utf8Path, error)) return false;
    version = d.version();
    return true;
}

}  // namespace setup
