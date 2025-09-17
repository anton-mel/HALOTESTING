#include "hdf5_writer.h"
#include <hdf5.h>
#include <filesystem>

Hdf5Writer::Hdf5Writer() : file_(nullptr), dset_codes_(nullptr), dset_uv_(nullptr), space_codes_(nullptr), space_uv_(nullptr), frameIndex_(0) {}

Hdf5Writer::~Hdf5Writer() { close(); }

bool Hdf5Writer::open(const std::string& path, const IntanHeaderInfo& info) {
    close();
    info_ = info;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    // Create file access property list with SWMR (Single Writer Multiple Reader) mode
    hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_fclose_degree(fapl, H5F_CLOSE_STRONG); // Force close to prevent locking issues
    
    hid_t file = H5Fcreate(path.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, fapl);
    H5Pclose(fapl);
    if (file < 0) return false;

    // Dataspace: unlimited frames x (streams*channels)
    hsize_t numSignals = static_cast<hsize_t>(info.streamCount) * info.channelCount;
    hsize_t dims[2] = {0, numSignals};
    hsize_t maxdims[2] = {H5S_UNLIMITED, numSignals};
    hid_t space = H5Screate_simple(2, dims, maxdims);

    // Chunking for append
    hid_t plist = H5Pcreate(H5P_DATASET_CREATE);
    hsize_t chunk[2] = {1024, numSignals};
    H5Pset_chunk(plist, 2, chunk);

    // Datatype: native little-endian uint16
    hid_t type = H5Tcopy(H5T_NATIVE_UINT16);

    hid_t dsetCodes = H5Dcreate2(file, "/samples_codes", type, space, H5P_DEFAULT, plist, H5P_DEFAULT);
    H5Pclose(plist);
    if (dsetCodes < 0) { H5Sclose(space); H5Fclose(file); return false; }

    // Second dataset: microvolts as float32
    hid_t space2 = H5Screate_simple(2, dims, maxdims);
    hid_t plist2 = H5Pcreate(H5P_DATASET_CREATE);
    H5Pset_chunk(plist2, 2, chunk);
    hid_t floatType = H5Tcopy(H5T_NATIVE_FLOAT);
    hid_t dsetUv = H5Dcreate2(file, "/samples_uV", floatType, space2, H5P_DEFAULT, plist2, H5P_DEFAULT);
    H5Pclose(plist2);
    if (dsetUv < 0) { H5Sclose(space2); H5Sclose(space); H5Dclose(dsetCodes); H5Fclose(file); return false; }

    // Attributes: metadata
    auto write_attr_u32 = [&](hid_t target, const char* name, uint32_t value){
        hid_t aspace = H5Screate(H5S_SCALAR);
        hid_t atype = H5Tcopy(H5T_NATIVE_UINT32);
        hid_t attr = H5Acreate2(target, name, atype, aspace, H5P_DEFAULT, H5P_DEFAULT);
        H5Awrite(attr, atype, &value);
        H5Aclose(attr); H5Sclose(aspace); H5Tclose(atype);
    };
    write_attr_u32(dsetCodes, "streamCount", info.streamCount);
    write_attr_u32(dsetCodes, "channelCount", info.channelCount);
    write_attr_u32(dsetCodes, "sampleRate", info.sampleRate);
    write_attr_u32(dsetUv, "streamCount", info.streamCount);
    write_attr_u32(dsetUv, "channelCount", info.channelCount);
    write_attr_u32(dsetUv, "sampleRate", info.sampleRate);

    file_ = reinterpret_cast<void*>(static_cast<intptr_t>(file));
    dset_codes_ = reinterpret_cast<void*>(static_cast<intptr_t>(dsetCodes));
    dset_uv_ = reinterpret_cast<void*>(static_cast<intptr_t>(dsetUv));
    space_codes_ = reinterpret_cast<void*>(static_cast<intptr_t>(space));
    space_uv_ = reinterpret_cast<void*>(static_cast<intptr_t>(space2));
    frameIndex_ = 0;
    return true;
}

void Hdf5Writer::close() {
    if (dset_codes_) { 
        H5Dclose(static_cast<hid_t>(reinterpret_cast<intptr_t>(dset_codes_))); 
        dset_codes_ = nullptr; 
    }
    if (dset_uv_) { 
        H5Dclose(static_cast<hid_t>(reinterpret_cast<intptr_t>(dset_uv_))); 
        dset_uv_ = nullptr; 
    }
    if (space_codes_) { 
        H5Sclose(static_cast<hid_t>(reinterpret_cast<intptr_t>(space_codes_))); 
        space_codes_ = nullptr; 
    }
    if (space_uv_) { 
        H5Sclose(static_cast<hid_t>(reinterpret_cast<intptr_t>(space_uv_))); 
        space_uv_ = nullptr; 
    }
    if (file_) { 
        // Flush the file before closing to ensure data is written
        H5Fflush(static_cast<hid_t>(reinterpret_cast<intptr_t>(file_)), H5F_SCOPE_GLOBAL);
        H5Fclose(static_cast<hid_t>(reinterpret_cast<intptr_t>(file_))); 
        file_ = nullptr; 
    }
}

bool Hdf5Writer::appendFrame(const std::vector<uint16_t>& codes, const std::vector<float>& microvolts) {
    if (!file_ || !dset_codes_ || !dset_uv_) return false;
    hsize_t numSignals = static_cast<hsize_t>(info_.streamCount) * info_.channelCount;
    if (codes.size() != numSignals || microvolts.size() != numSignals) return false;

    // Extend dataset by 1 frame
    hsize_t newdims[2] = {frameIndex_ + 1, numSignals};
    hid_t dsetC = static_cast<hid_t>(reinterpret_cast<intptr_t>(dset_codes_));
    hid_t dsetU = static_cast<hid_t>(reinterpret_cast<intptr_t>(dset_uv_));
    H5Dset_extent(dsetC, newdims);
    H5Dset_extent(dsetU, newdims);

    // Select the hyperslab for the new frame
    hid_t fspaceC = H5Dget_space(dsetC);
    hsize_t start[2] = {frameIndex_, 0};
    hsize_t count[2] = {1, numSignals};
    H5Sselect_hyperslab(fspaceC, H5S_SELECT_SET, start, nullptr, count, nullptr);

    // Memory space for the frame
    hsize_t mdims[2] = {1, numSignals};
    hid_t mspaceC = H5Screate_simple(2, mdims, nullptr);

    // Write
    herr_t s1 = H5Dwrite(dsetC, H5T_NATIVE_UINT16, mspaceC, fspaceC, H5P_DEFAULT, codes.data());
    H5Sclose(mspaceC);
    H5Sclose(fspaceC);
    if (s1 < 0) return false;

    // UV write
    hid_t fspaceU = H5Dget_space(dsetU);
    H5Sselect_hyperslab(fspaceU, H5S_SELECT_SET, start, nullptr, count, nullptr);
    hid_t mspaceU = H5Screate_simple(2, mdims, nullptr);
    herr_t s2 = H5Dwrite(dsetU, H5T_NATIVE_FLOAT, mspaceU, fspaceU, H5P_DEFAULT, microvolts.data());
    H5Sclose(mspaceU);
    H5Sclose(fspaceU);
    if (s2 < 0) return false;
    
    // Flush after each frame to ensure data is persisted
    H5Fflush(static_cast<hid_t>(reinterpret_cast<intptr_t>(file_)), H5F_SCOPE_GLOBAL);
    
    ++frameIndex_;
    return true;
}
