#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <systemd/sd-bus.h>

#define MSI_SHIFT_MODE_PATH "/sys/devices/platform/msi-ec/shift_mode"

void set_msi_shift_mode(const char *ubuntu_profile) {
    const char *msi_mode = NULL;
    
    if (strcmp(ubuntu_profile, "performance") == 0) {
        msi_mode = "turbo";
    } else if (strcmp(ubuntu_profile, "balanced") == 0) {
        msi_mode = "comfort";
    } else if (strcmp(ubuntu_profile, "power-saver") == 0) {
        msi_mode = "eco";
    } else {
        return;
    }

    // Read current mode to avoid writing if already set
    char current_mode[64];
    memset(current_mode, 0, sizeof(current_mode));
    FILE *rf = fopen(MSI_SHIFT_MODE_PATH, "r");
    if (rf) {
        if (fgets(current_mode, sizeof(current_mode) - 1, rf)) {
            current_mode[strcspn(current_mode, "\n")] = 0;
        }
        fclose(rf);
    }

    if (strcmp(current_mode, msi_mode) == 0) {
        return; // Already in sync, avoid writing again
    }

    FILE *wf = fopen(MSI_SHIFT_MODE_PATH, "w");
    if (!wf) {
        perror("msi-power-sync: failed to open shift_mode for writing");
        return;
    }
    
    if (fputs(msi_mode, wf) == EOF) {
        perror("msi-power-sync: failed to write shift_mode");
    } else {
        printf("msi-power-sync: successfully set shift_mode to %s\n", msi_mode);
    }
    fclose(wf);
}

void handle_msi_shift_mode_changed(sd_bus *bus, const char *msi_mode) {
    const char *ubuntu_profile = NULL;

    if (strcmp(msi_mode, "turbo") == 0) {
        ubuntu_profile = "performance";
    } else if (strcmp(msi_mode, "comfort") == 0) {
        ubuntu_profile = "balanced";
    } else if (strcmp(msi_mode, "eco") == 0) {
        ubuntu_profile = "power-saver";
    } else {
        return; // Ignore other modes
    }

    // Read current active profile from D-Bus to avoid infinite loop
    sd_bus_error error = SD_BUS_ERROR_NULL;
    char *current_profile = NULL;
    int r = sd_bus_get_property_string(
        bus,
        "net.hadess.PowerProfiles",
        "/net/hadess/PowerProfiles",
        "net.hadess.PowerProfiles",
        "ActiveProfile",
        &error,
        &current_profile
    );

    if (r >= 0) {
        if (strcmp(current_profile, ubuntu_profile) == 0) {
            free(current_profile);
            return; // Already in sync
        }
        free(current_profile);
    } else {
        sd_bus_error_free(&error);
    }

    printf("msi-power-sync: Syncing Ubuntu power profile to match EC: setting to %s\n", ubuntu_profile);
    
    error = SD_BUS_ERROR_NULL;
    r = sd_bus_set_property(
        bus,
        "net.hadess.PowerProfiles",
        "/net/hadess/PowerProfiles",
        "net.hadess.PowerProfiles",
        "ActiveProfile",
        &error,
        "s",
        ubuntu_profile
    );

    if (r < 0) {
        fprintf(stderr, "msi-power-sync: Failed to set ActiveProfile on D-Bus: %s\n", error.message);
        sd_bus_error_free(&error);
    }
}

// Callback for DBus PropertiesChanged signal
static int handle_properties_changed(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *interface;
    int r;

    r = sd_bus_message_read(m, "s", &interface);
    if (r < 0) {
        fprintf(stderr, "Failed to parse signal interface: %s\n", strerror(-r));
        return r;
    }

    if (strcmp(interface, "net.hadess.PowerProfiles") != 0) {
        return 0; // Not our interface
    }

    r = sd_bus_message_enter_container(m, 'a', "{sv}");
    if (r < 0) {
        fprintf(stderr, "Failed to enter container: %s\n", strerror(-r));
        return r;
    }

    while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
        const char *property;
        r = sd_bus_message_read(m, "s", &property);
        if (r < 0) {
            fprintf(stderr, "Failed to read property name: %s\n", strerror(-r));
            return r;
        }

        if (strcmp(property, "ActiveProfile") == 0) {
            const char *val;
            r = sd_bus_message_enter_container(m, 'v', "s");
            if (r < 0) {
                fprintf(stderr, "Failed to enter variant: %s\n", strerror(-r));
                return r;
            }
            r = sd_bus_message_read(m, "s", &val);
            if (r < 0) {
                fprintf(stderr, "Failed to read variant value: %s\n", strerror(-r));
                return r;
            }
            printf("msi-power-sync: Ubuntu power profile changed to %s\n", val);
            set_msi_shift_mode(val);
            sd_bus_message_exit_container(m); // Exit variant
            sd_bus_message_exit_container(m); // Exit dict entry
            break;
        }

        sd_bus_message_skip(m, "v");
        sd_bus_message_exit_container(m);
    }
    
    sd_bus_message_exit_container(m);
    return 0;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    sd_bus *bus = NULL;
    int ec_fd = -1;
    int r;

    r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto finish;
    }

    // Query initial state
    sd_bus_error error = SD_BUS_ERROR_NULL;
    char *initial_profile = NULL;
    r = sd_bus_get_property_string(
        bus,
        "net.hadess.PowerProfiles",
        "/net/hadess/PowerProfiles",
        "net.hadess.PowerProfiles",
        "ActiveProfile",
        &error,
        &initial_profile
    );

    if (r < 0) {
        fprintf(stderr, "Failed to get initial ActiveProfile: %s\n", error.message);
        sd_bus_error_free(&error);
    } else {
        printf("msi-power-sync: Initial Ubuntu power profile is %s\n", initial_profile);
        set_msi_shift_mode(initial_profile);
        free(initial_profile);
    }

    r = sd_bus_match_signal(
        bus,
        NULL,
        "net.hadess.PowerProfiles",
        "/net/hadess/PowerProfiles",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        handle_properties_changed,
        NULL
    );
    if (r < 0) {
        fprintf(stderr, "Failed to register signal match: %s\n", strerror(-r));
        goto finish;
    }

    int bus_fd = sd_bus_get_fd(bus);
    if (bus_fd < 0) {
        fprintf(stderr, "Failed to get bus fd: %d\n", bus_fd);
        goto finish;
    }

    ec_fd = open(MSI_SHIFT_MODE_PATH, O_RDONLY);
    if (ec_fd < 0) {
        perror("Failed to open shift_mode for polling");
        goto finish;
    }

    // Perform initial read to clear any pending events
    char initial_buf[64];
    memset(initial_buf, 0, sizeof(initial_buf));
    if (read(ec_fd, initial_buf, sizeof(initial_buf) - 1) > 0) {
        initial_buf[strcspn(initial_buf, "\n")] = 0;
    }
    lseek(ec_fd, 0, SEEK_SET);

    struct pollfd fds[2];
    fds[0].fd = bus_fd;
    fds[0].events = POLLIN;
    fds[1].fd = ec_fd;
    fds[1].events = POLLPRI | POLLERR;

    printf("msi-power-sync: Daemon started successfully, monitoring power profiles...\n");

    for (;;) {
        r = sd_bus_process(bus, NULL);
        if (r < 0) {
            fprintf(stderr, "Failed to process bus events: %s\n", strerror(-r));
            goto finish;
        }
        if (r > 0) {
            continue;
        }

        r = poll(fds, 2, -1);
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("Poll failed");
            goto finish;
        }

        if (fds[1].revents & (POLLPRI | POLLERR)) {
            char val[64];
            memset(val, 0, sizeof(val));
            lseek(ec_fd, 0, SEEK_SET);
            if (read(ec_fd, val, sizeof(val) - 1) > 0) {
                val[strcspn(val, "\n")] = 0;
                printf("msi-power-sync: EC shift_mode changed to %s\n", val);
                handle_msi_shift_mode_changed(bus, val);
            }
        }
    }

finish:
    if (ec_fd >= 0) close(ec_fd);
    sd_bus_unref(bus);
    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
