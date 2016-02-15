/*
 * VDR Channels proxy.
 */

#ifndef RoboTVCHANNELS_H_
#define RoboTVCHANNELS_H_

#include <vdr/channels.h>

class RoboTVChannels: public cRwLock {
private:

    cChannels* m_channels;

    uint64_t m_hash;

    cChannels* reorder(cChannels* channels);

    bool read(FILE* f, cChannels* channels);

    bool write(FILE* f, cChannels* channels);

    uint64_t getChannelsHash(cChannels* channels);

protected:

    RoboTVChannels();

public:

    static RoboTVChannels& instance();

    /**
     * Calculates the VDR Channels hash and compares with the cached value
     * (channelsHash). If the value has changed an the ReorderCmd configuration
     * parameter is specified - reorder the VDR Channels list with the ReorderCmd
     * command and cache the reordered list.
     *
     * Returns the calculated hash value.
     *
     * TODO: Think about replacing hash with checksum.
     */
    uint64_t checkUpdates();

    /**
     * Returns reference to either reordered list (if ReorderCmd is specified),
     * or to the VDR Channels.
     *
     * NOTE: Lock before calling this method.
     */
    cChannels* get();

    /**
     * Returns the channels hash calculated on the prev. CheckUpdates().
     */
    uint64_t getHash();

    /**
     * Lock both this instance an the referencing channels list.
     */
    bool lock(bool Write, int TimeoutMs = 0);

    /**
     * Unlock this instance an the referencing channels list.
     */
    void unlock(void);
};

#endif /* RoboTVCHANNELS_H_ */
