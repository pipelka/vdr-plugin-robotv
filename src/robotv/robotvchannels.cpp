#include <sys/wait.h>
#include "config/config.h"
#include "tools/hash.h"
#include "vdr/tools.h"
#include "robotvchannels.h"

RoboTVChannels::RoboTVChannels() {
    Channels.Lock(false);
    m_channels = reorder(&Channels);
    m_hash = getChannelsHash(&Channels);
    Channels.Unlock();
}

RoboTVChannels& RoboTVChannels::instance() {
    static RoboTVChannels channels;
    return channels;
}

uint64_t RoboTVChannels::checkUpdates() {
    cRwLock::Lock(false);
    Channels.Lock(false);

    cChannels* oldChannels = m_channels;
    uint64_t oldHash = m_hash;
    uint64_t newHash = getChannelsHash(&Channels);

    if(newHash == oldHash) {
        Channels.Unlock();
        cRwLock::Unlock();
        return oldHash;
    }

    cRwLock::Unlock();
    cRwLock::Lock(true);

    if((m_hash == oldHash) && (m_channels == oldChannels)) {
        if(m_channels != &Channels) {
            delete m_channels;
        }

        m_channels = reorder(&Channels);
        m_hash = newHash;
    }
    else {
        // Seems another thread has already updated the hash.
        newHash = m_hash;
    }

    Channels.Unlock();
    cRwLock::Unlock();
    return newHash;
}

cChannels* RoboTVChannels::get() {
    return m_channels;
}

cChannels* RoboTVChannels::reorder(cChannels* channels) {
    std::string reorderCmd = RoboTVServerConfig::instance().ReorderCmd;

    if(reorderCmd.empty()) {
        return channels;
    }

    pid_t pid;
    int status;
    int input[2];
    int output[2];

    if(pipe(input) == -1) {
        ERRORLOG("Failed to create pipe");
        return channels;
    }

    if(pipe(output) == -1) {
        ERRORLOG("Failed to create pipe");
        close(input[0]);
        close(input[1]);
        return channels;
    }

    switch(pid = fork()) {
        case -1:
            ERRORLOG("Failed to fork new process");
            close(input[0]);
            close(input[1]);
            close(output[0]);
            close(output[1]);
            return channels;

        case 0:
            // Close unused descriptors
            close(input[1]);
            close(output[0]);

            // Redirect streams
            dup2(input[0], STDIN_FILENO);
            dup2(output[1], STDOUT_FILENO);

            INFOLOG(
                "Reordering %i channels with command '%s'", channels->Count(), reorderCmd.c_str());
            status = system(reorderCmd.c_str());

            if(status != 0) {
                ERRORLOG(
                    "Command: %s failed with exit code %i", reorderCmd.c_str(), status);
            }

            close(input[0]);
            close(output[1]);
            _exit(status);

        default:
            FILE* f;
            bool result;
            cChannels* reordered;

            // Close unused descriptors
            close(input[0]);
            close(output[1]);

            // Write channels
            f = fdopen(input[1], "w");
            result = write(f, channels);
            fclose(f);
            close(input[1]);

            if(!result) {
                ERRORLOG("Failed to write channels to the command's input");
                close(output[0]);
                return channels;
            }

            // Load channels
            f = fdopen(output[0], "r");
            reordered = new cChannels();
            result = read(f, reordered);
            fclose(f);
            close(output[0]);
            waitpid(pid, &status, 0);

            if(WEXITSTATUS(status) != 0) {
                ERRORLOG("Returning original channels due to reorder failure");
                delete reordered;
                return channels;
            }

            if(!result) {
                ERRORLOG("Failed to read channels from the command's output");
                delete reordered;
                return channels;
            }

            INFOLOG("Loaded %i channels", reordered->Count());
            return reordered;
    }
}

uint64_t RoboTVChannels::getChannelsHash(cChannels* channels) {
    uint64_t hash = 0;
    uint64_t count = 0;

    for(cChannel* c = channels->First(); c != NULL; c = channels->Next(c)) {
        count++;
        hash ^= CreateChannelUID(c);
    }

    return (count << 32) | hash;
}

bool RoboTVChannels::read(FILE* f, cChannels* channels) {
    cReadLine ReadLine;

    for(char* line = ReadLine.Read(f); line != NULL; line = ReadLine.Read(f)) {
        char* hash = strchr(line, '#');

        if(hash != NULL) {
            *hash = 0;
        }

        stripspace(line);

        if(!isempty(line)) {
            cChannel* c = new cChannel();

            if(c->Parse(line)) {
                channels->Add(c);
            }
            else {
                delete c;
                ERRORLOG("Invalid channel: %s", line);
                return false;
            }
        }
    }

    channels->ReNumber();
    return true;
}

bool RoboTVChannels::write(FILE* f, cChannels* channels) {
    for(cChannel* c = channels->First(); c != NULL; c = channels->Next(c)) {
        if(!c->Save(f)) {
            return false;
        }
    }

    return true;
}

bool RoboTVChannels::lock(bool Write, int TimeoutMs) {
    if(cRwLock::Lock(Write, TimeoutMs)) {
        if(get()->Lock(Write, TimeoutMs)) {
            return true;
        }
        else {
            cRwLock::Unlock();
        }
    }

    return false;
}

void RoboTVChannels::unlock(void) {
    get()->Unlock();
    cRwLock::Unlock();
}
