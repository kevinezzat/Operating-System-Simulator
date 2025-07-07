#include <gtk/gtk.h>
#include <gtk/gtkentry.h>
#include <string.h>
#include "main.h"

#define NUM_QUEUES 4

static GtkWidget *window = NULL;
static guint auto_timer_id = 0;
static gboolean auto_execution_active = FALSE;

// Widgets
static GtkWidget *overview_label;
static GtkWidget *process_list_view;
static GtkWidget *queue_list_view;
static GtkWidget *scheduler_dropdown;
static GtkWidget *quantum_spin;
static GtkWidget *start_button;
static GtkWidget *stop_button;
static GtkWidget *reset_button;
static GtkWidget *step_button;
static GtkWidget *auto_button;
static GtkWidget *mutex_grid;
static GtkWidget *log_text_view;
static GtkWidget *add_process_button;
static GtkWidget *arrival_spin;
static GtkWidget *log_entry;

// List stores for dynamic data
static GListStore *process_store;
static GListStore *queue_store;

static GListStore *memory_store = NULL;

static GtkWidget *memory_grid_viewer = NULL;

static gboolean update_log_idle(gpointer user_data) {
    const char *message = (const char *)user_data;
    if (log_text_view != NULL) {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_text_view));
        if (buffer != NULL) {
            GtkTextIter end;
            gtk_text_buffer_get_end_iter(buffer, &end);
            gtk_text_buffer_insert(buffer, &end, message, -1);
            gtk_text_buffer_insert(buffer, &end, "\n", -1);
            gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(log_text_view), &end, 0.0, TRUE, 0.0, 1.0);
            gtk_widget_queue_draw(log_text_view);
        }
    }
    g_free(user_data);
    return G_SOURCE_REMOVE;
}

void append_log(SimulationState *state, const char *message) {
    char formatted_message[1024];
    if (state->runningPid > 0) {
        snprintf(formatted_message, sizeof(formatted_message), 
                "[Cycle %d] [PID %d] %s", 
                state->clockCycle,
                state->runningPid,
                message);
    } else {
        snprintf(formatted_message, sizeof(formatted_message), 
                "%s", 
                message);
    }
    strncat(state->log, formatted_message, MAX_LOG_LENGTH - strlen(state->log) - 1);
    strncat(state->log, "\n", MAX_LOG_LENGTH - strlen(state->log) - 1);
    char *message_copy = g_strdup(formatted_message);
    g_idle_add(update_log_idle, message_copy);
}

void update_process_list() {
    if (!G_IS_LIST_STORE(process_store)) return;
    g_list_store_remove_all(process_store);
    
    for (int i = 0; i < sim_state.numProcesses; i++) {
        ProcessInfo *info = &sim_state.processes[i];
        char *display_text = g_strdup_printf(
            "PID: %d | State: %s | Queue: %d | PC: %d | Memory: %d-%d | Time in Queue: %d",
            info->pid,
            info->state,
            info->priority,
            info->pc,
            info->lowerBound,
            info->upperBound,
            info->timeInQueue);
 
        GObject *item = G_OBJECT(gtk_string_object_new(display_text));
        g_list_store_append(process_store, item);
        g_object_unref(item);
        g_free(display_text);
    }
}

void update_queue_list() {
    if (!G_IS_LIST_STORE(queue_store)) return;
    g_list_store_remove_all(queue_store);
    
    if (sim_state.runningPid > 0) {
        for (int i = 0; i < sim_state.numProcesses; i++) {
            if (sim_state.processes[i].pid == sim_state.runningPid) {
                char *display_text = g_strdup_printf(
                    "RUNNING | PID: %d | Time: %d | Queue: %d",
                    sim_state.processes[i].pid,
                    sim_state.processes[i].timeInQueue,
                    sim_state.processes[i].priority);
                GObject *item = G_OBJECT(gtk_string_object_new(display_text));
                g_list_store_append(queue_store, item);
                g_object_unref(item);
                g_free(display_text);
                break;
            }
        }
    }

    static Queue mlfq_queues[NUM_QUEUES];
    static int initialized = 0;
    if (!initialized) {
        for (int i = 0; i < NUM_QUEUES; i++) {
            initializeQueue(&mlfq_queues[i]);
        }
        initialized = 1;
    }

    for (int i = 0; i < NUM_QUEUES; i++) {
        while (!isEmpty(&mlfq_queues[i])) {
            dequeue(&mlfq_queues[i]);
        }
    }

    for (int i = 0; i < sim_state.numProcesses; i++) {
        ProcessInfo *info = &sim_state.processes[i];
        if (info && strcmp(info->state, "Ready") == 0) {
            int queue_index = info->priority;
            if (queue_index >= 0 && queue_index < NUM_QUEUES) {
                enqueue(&mlfq_queues[queue_index], info->pid);
            }
        }
    }

    for (int q = 0; q < NUM_QUEUES; q++) {
        if (isEmpty(&mlfq_queues[q])) continue;

        Queue tempQueue;
        initializeQueue(&tempQueue);
        
        int count = countQueueElements(&mlfq_queues[q]);
        for (int i = 0; i < count; i++) {
            int pid = dequeue(&mlfq_queues[q]);
            if (pid > 0) {
                enqueue(&tempQueue, pid);
                enqueue(&mlfq_queues[q], pid);
            }
        }

        while (!isEmpty(&tempQueue)) {
            int pid = dequeue(&tempQueue);
            if (pid <= 0) continue;

            for (int i = 0; i < sim_state.numProcesses; i++) {
                if (sim_state.processes[i].pid == pid) {
                    char *display_text = g_strdup_printf(
                        "MLFQ Queue %d | PID: %d | Time: %d | Queue: %d",
                        q,
                        sim_state.processes[i].pid,
                        sim_state.processes[i].timeInQueue,
                        q);
                    GObject *item = G_OBJECT(gtk_string_object_new(display_text));
                    g_list_store_append(queue_store, item);
                    g_object_unref(item);
                    g_free(display_text);
                    break;
                }
            }
        }
    }

    if (!isEmpty(&sim_state.blockedQueue)) {
        Queue tempQueue;
        initializeQueue(&tempQueue);
        
        int count = countQueueElements(&sim_state.blockedQueue);
        for (int i = 0; i < count; i++) {
            int pid = dequeue(&sim_state.blockedQueue);
            if (pid > 0) {
                enqueue(&tempQueue, pid);
                enqueue(&sim_state.blockedQueue, pid);
            }
        }

        while (!isEmpty(&tempQueue)) {
            int pid = dequeue(&tempQueue);
            if (pid <= 0) continue;

            for (int i = 0; i < sim_state.numProcesses; i++) {
                if (sim_state.processes[i].pid == pid) {
                    char *display_text = g_strdup_printf(
                        "BLOCKED | PID: %d | Time: %d | Queue: %d",
                        sim_state.processes[i].pid,
                        sim_state.processes[i].timeInQueue,
                        sim_state.processes[i].priority);
                    GObject *item = G_OBJECT(gtk_string_object_new(display_text));
                    g_list_store_append(queue_store, item);
                    g_object_unref(item);
                    g_free(display_text);
                    break;
                }
            }
        }
    }
}

static void setup_list_item(GtkListItemFactory *factory, GtkListItem *item, gpointer user_data) {
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_list_item_set_child(item, label);
}

static void bind_list_item(GtkListItemFactory *factory, GtkListItem *item, gpointer user_data) {
    GtkWidget *label = gtk_list_item_get_child(item);
    GtkStringObject *string_obj = gtk_list_item_get_item(item);
    if (string_obj) {
        const char *text = gtk_string_object_get_string(string_obj);
        gtk_label_set_text(GTK_LABEL(label), text);
    }
}

static void update_mutex_grid() {
    for (int i = 0; i < 3; i++) {
        GtkWidget *status_label = gtk_grid_get_child_at(GTK_GRID(mutex_grid), 1, i + 1);
        if (!status_label) {
            status_label = gtk_label_new("");
            gtk_widget_add_css_class(status_label, "resource-status");
            gtk_grid_attach(GTK_GRID(mutex_grid), status_label, 1, i + 1, 1, 1);
        }
        
        GtkWidget *owner_label = gtk_grid_get_child_at(GTK_GRID(mutex_grid), 2, i + 1);
        if (!owner_label) {
            owner_label = gtk_label_new("");
            gtk_widget_add_css_class(owner_label, "resource-status");
            gtk_grid_attach(GTK_GRID(mutex_grid), owner_label, 2, i + 1, 1, 1);
        }

        if (sim_state.mutexes[i].locked) {
            gtk_label_set_text(GTK_LABEL(status_label), "Locked");
            gtk_widget_remove_css_class(status_label, "resource-unlocked");
            gtk_widget_add_css_class(status_label, "resource-locked");
        } else {
            gtk_label_set_text(GTK_LABEL(status_label), "Unlocked");
            gtk_widget_remove_css_class(status_label, "resource-locked");
            gtk_widget_add_css_class(status_label, "resource-unlocked");
        }

        if (sim_state.mutexes[i].ownerPid > 0) {
            char owner_text[20];
            snprintf(owner_text, sizeof(owner_text), "PID %d", sim_state.mutexes[i].ownerPid);
            gtk_label_set_text(GTK_LABEL(owner_label), owner_text);
        } else {
            gtk_label_set_text(GTK_LABEL(owner_label), "None");
        }
    }
}

void update_overview() {
    int used_memory = 0;
    for (int i = 0; i < sim_state.numProcesses; i++) {
        if (strcmp(sim_state.processes[i].state, "Terminated") != 0) {
            used_memory += (sim_state.processes[i].upperBound - sim_state.processes[i].lowerBound + 1);
        }
    }

    char overview[256];
    snprintf(overview, sizeof(overview), 
             "Total Processes: %d | Clock Cycle: %d | Active Scheduler: %s | Available Memory: %d",
             sim_state.numProcesses, 
             sim_state.clockCycle, 
             sim_state.schedulerType,
             MEMORY_SIZE - used_memory);
    gtk_label_set_text(GTK_LABEL(overview_label), overview);
}

static void update_memory_grid_viewer() {
    if (!memory_grid_viewer) return;
    for (int i = 0; i < MEMORY_SIZE; i++) {
        int col = i / 20;
        int row = i % 20;
        GtkWidget *label = gtk_grid_get_child_at(GTK_GRID(memory_grid_viewer), col, row);
        if (!label) continue;
        char display[192];
        snprintf(display, sizeof(display), "<span foreground='#569cd6' weight='bold'>%d:</span> %s", i, sim_state.memory[i][0] ? sim_state.memory[i] : "Empty");
        gtk_label_set_markup(GTK_LABEL(label), display);
    }
}

static gboolean update_gui_idle(gpointer user_data) {
    update_overview();
    update_process_list();
    update_queue_list();
    update_mutex_grid();
    update_memory_grid_viewer();
    return G_SOURCE_REMOVE;
}

void update_gui(SimulationState *state) {
    g_idle_add(update_gui_idle, NULL);
}

static void on_scheduler_changed(GtkDropDown *dropdown, gpointer user_data) {
    GtkStringObject *selected = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(dropdown));
    const char *text = gtk_string_object_get_string(selected);
    strncpy(sim_state.schedulerType, text, sizeof(sim_state.schedulerType) - 1);
    sim_state.schedulerType[sizeof(sim_state.schedulerType) - 1] = '\0';
    append_log(&sim_state, g_strdup_printf("Scheduler changed to %s", sim_state.schedulerType));
}

static void on_quantum_changed(GtkSpinButton *spin, gpointer user_data) {
    sim_state.rrQuantum = gtk_spin_button_get_value_as_int(spin);
    append_log(&sim_state, g_strdup_printf("Round-Robin quantum set to %d", sim_state.rrQuantum));
}

static void on_start_clicked(GtkButton *button, gpointer user_data) {
    if (auto_timer_id == 0) {
        auto_timer_id = g_timeout_add(1000, (GSourceFunc)run_simulation_cycle, &sim_state);
        append_log(&sim_state, "Simulation started");
    }
}

static void on_stop_clicked(GtkButton *button, gpointer user_data) {
    if (auto_timer_id != 0) {
        g_source_remove(auto_timer_id);
        auto_timer_id = 0;
        append_log(&sim_state, "Simulation stopped");
    }
}

static void on_reset_clicked(GtkButton *button, gpointer user_data) {
    reset_simulation(&sim_state);
    append_log(&sim_state, "Simulation reset");
}

enum { STEP_COMMAND, OTHER_COMMAND };

static int parse_command(const char *input) {
    if (g_strcmp0(input, "step") == 0) return STEP_COMMAND;
    return OTHER_COMMAND;
}

static void on_step_clicked(GtkButton *button, gpointer user_data) {
    run_simulation_cycle(&sim_state);
    update_gui(&sim_state);
}

static gboolean run_simulation_cycle_idle(gpointer user_data) {
    run_simulation_cycle(&sim_state);
    update_gui(&sim_state);

    if (sim_state.waiting_for_input_pid > 0) {
        auto_timer_id = 0;
        gtk_button_set_label(GTK_BUTTON(auto_button), "Auto Execute");
        append_log(&sim_state, g_strdup_printf("Paused for input for PID %d, variable %s",
                                               sim_state.waiting_for_input_pid,
                                               sim_state.waiting_for_input_var));
        return G_SOURCE_REMOVE;
    }

    int all_finished = 1;
    for (int i = 0; i < sim_state.numProcesses; i++) {
        if (strcmp(sim_state.processes[i].state, "Terminated") != 0) {
            all_finished = 0;
            break;
        }
    }

    if (all_finished) {
        auto_timer_id = 0;
        auto_execution_active = FALSE;
        gtk_button_set_label(GTK_BUTTON(auto_button), "Auto Execute");
        append_log(&sim_state, "Auto execution stopped - all processes finished");
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

static void on_auto_clicked(GtkButton *button, gpointer user_data) {
    if (auto_timer_id == 0) {
        auto_timer_id = g_idle_add(run_simulation_cycle_idle, NULL);
        auto_execution_active = TRUE;
        gtk_button_set_label(button, "Pause Auto");
        append_log(&sim_state, "Auto execution started");
    } else {
        g_source_remove(auto_timer_id);
        auto_timer_id = 0;
        auto_execution_active = FALSE;
        gtk_button_set_label(button, "Auto Execute");
        append_log(&sim_state, "Auto execution paused");
    }
}

static void on_file_dialog_response(GObject *source_object, GAsyncResult *result, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_open_finish(dialog, result, &error);
    
    if (error != NULL) {
        g_warning("Error opening file: %s", error->message);
        g_error_free(error);
        return;
    }

    if (file != NULL) {
        char *filename = g_file_get_path(file);
        int arrival = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(arrival_spin));
        add_process(&sim_state, filename, arrival);
        append_log(&sim_state, g_strdup_printf("Added process from %s with arrival time %d", filename, arrival));
        g_free(filename);
        g_object_unref(file);
    }
}

static void on_add_process_clicked(GtkButton *button, gpointer user_data) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Select Process File");
    
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Text Files");
    gtk_file_filter_add_pattern(filter, "*.txt");
    gtk_file_dialog_set_default_filter(dialog, filter);
    
    gtk_file_dialog_open(dialog, GTK_WINDOW(window), NULL, on_file_dialog_response, NULL);
    g_object_unref(dialog);
}

static void on_log_entry_activate(GtkEntry *entry, gpointer user_data) {
    const char *input = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (input && strlen(input) > 0) {
        if (sim_state.waiting_for_input_pid > 0 && strlen(sim_state.waiting_for_input_var) > 0) {
            int pid = sim_state.waiting_for_input_pid;
            const char *var = sim_state.waiting_for_input_var;
            updateVariable(pid, var, input);
            append_log(&sim_state, g_strdup_printf("PID %d: Assigned %s = %s (from GUI input)", pid, var, input));
            sim_state.waiting_for_input_pid = 0;
            sim_state.waiting_for_input_var[0] = '\0';
            if (auto_execution_active && auto_timer_id == 0) {
                auto_timer_id = g_idle_add(run_simulation_cycle_idle, NULL);
                gtk_button_set_label(GTK_BUTTON(auto_button), "Pause Auto");
                append_log(&sim_state, "Auto execution resumed after input");
            }
        } else {
            append_log(&sim_state, input);
            int cmd = parse_command(input);
            if (cmd == STEP_COMMAND) {
                run_simulation_cycle(&sim_state);
                update_gui(&sim_state);
            }
        }
        gtk_editable_set_text(GTK_EDITABLE(entry), "");
    }
}

static void setup_theme() {
    GtkCssProvider *provider = gtk_css_provider_new();
    const char *css = 
        "window { background-color: #1e1e1e; color: #d4d4d4; }"
        "frame { background-color: #252526; border: 1px solid #3c3c3c; }"
        "label { color: #d4d4d4; }"
        "scrolledwindow { background-color: #1e1e1e; }"
        "listview { background-color: #1e1e1e; }"
        "listview row { padding: 5px; }"
        "listview row:hover { background-color: #2a2d2e; }"
        ".resource-header { font-weight: bold; color: #569cd6; padding: 5px; }"
        "frame > label { color: #569cd6; font-weight: bold; }"
        ".resource-status { padding: 5px; }"
        ".resource-locked { color: #f14c4c; }"
        ".resource-unlocked { color: #6a9955; }"
        ".memory-pcb { background-color: #2d2d2d; color: #d4d4d4; padding: 2px; border: 1px solid #3c3c3c; }"
        ".memory-data { background-color: #1e1e1e; color: #d4d4d4; padding: 2px; border: 1px solid #3c3c3c; }"
        ".memory-empty { background-color: #1e1e1e; color: #808080; padding: 2px; border: 1px solid #3c3c3c; }"
        ".memory-pcb:hover { background-color: #2a2d2e; }"
        ".memory-data:hover { background-color: #2a2d2e; }"
        "button:not(:disabled) { background-color: #007acc; color: #ffffff; padding: 8px 12px; border: 1.5px solid #005a9e; border-radius: 3px; font-weight: bold; }"
        "button:not(:disabled):hover { background-color: #2491e7; color: #ffffff; border: 1.5px solid #005a9e; }"
        "button:not(:disabled):active { background-color: #005a9e; color: #ffffff; border: 1.5px solid #003366; }"
        "button:disabled { background-color: #2491e7; color: #ffffff; border: 1.5px solid #3c3c3c; }"
        "entry { background-color: #3c3c3c; color: #d4d4d4; padding: 5px; border: 1px solid #3c3c3c; }"
        "entry:focus { border: 1px solid #0e639c; }"
        "combobox { background-color: #3c3c3c; color: #d4d4d4; }"
        "spinbutton { background-color: #3c3c3c; color: #d4d4d4; }"
        ".log-text-large { font-family: 'Consolas', monospace; font-size: 14px; background-color: #1e1e1e; color: #d4d4d4; }"
        "grid { background-color: #1e1e1e; }"
        "grid > label { color: #d4d4d4; }"
        ".memory-cell { background-color: #23272e; color: #d4d4d4; border-radius: 6px; border: 1px solid #333; padding: 4px 8px; margin: 2px; font-family: 'Fira Mono', 'Consolas', monospace; }"
        ".memory-pid { color: #569cd6; font-weight: bold; }";
        
    gtk_css_provider_load_from_string(provider, css);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);
}

static void setup_memory_grid_viewer(GtkWidget *container) {
    GtkWidget *frame = gtk_frame_new("Memory Viewer");
    gtk_box_append(GTK_BOX(container), frame);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(scroll, -1, 250);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_frame_set_child(GTK_FRAME(frame), scroll);

    memory_grid_viewer = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(memory_grid_viewer), 8);
    gtk_grid_set_row_spacing(GTK_GRID(memory_grid_viewer), 4);
    gtk_widget_set_hexpand(memory_grid_viewer, TRUE);
    gtk_widget_set_vexpand(memory_grid_viewer, TRUE);

    for (int i = 0; i < MEMORY_SIZE; i++) {
        int col = i / 20;
        int row = i % 20;
        char display[192];
        snprintf(display, sizeof(display), "<span foreground='#569cd6' weight='bold'>%d:</span> %s", i, sim_state.memory[i][0] ? sim_state.memory[i] : "Empty");
        GtkWidget *label = gtk_label_new(NULL);
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_label_set_markup(GTK_LABEL(label), display);
        gtk_widget_add_css_class(label, "memory-cell");
        gtk_widget_set_hexpand(label, TRUE);
        gtk_widget_set_vexpand(label, FALSE);
        gtk_grid_attach(GTK_GRID(memory_grid_viewer), label, col, row, 1, 1);
    }
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), memory_grid_viewer);
}

static void setup_gui(GtkApplication *app, gpointer user_data) {
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "OS Scheduler Simulator");
    gtk_window_set_default_size(GTK_WINDOW(window), 1400, 900);

    setup_theme();

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(vbox, 10);
    gtk_widget_set_margin_end(vbox, 10);
    gtk_widget_set_margin_top(vbox, 10);
    gtk_widget_set_margin_bottom(vbox, 10);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(vbox), main_box);

    GtkWidget *left_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_append(GTK_BOX(main_box), left_panel);

    GtkWidget *right_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_append(GTK_BOX(main_box), right_panel);

    GtkWidget *right_top = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *right_bottom = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_append(GTK_BOX(right_panel), right_top);
    gtk_box_append(GTK_BOX(right_panel), right_bottom);

    gtk_widget_set_vexpand(right_panel, TRUE);
    gtk_widget_set_vexpand(right_bottom, TRUE);

    GtkWidget *dashboard_frame = gtk_frame_new("Main Dashboard");
    GtkWidget *dashboard_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_frame_set_child(GTK_FRAME(dashboard_frame), dashboard_box);

    GtkWidget *overview_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    overview_label = gtk_label_new("Processes: 0 | Cycle: 0 | Scheduler: mlfq");
    gtk_label_set_xalign(GTK_LABEL(overview_label), 0);
    gtk_box_append(GTK_BOX(overview_box), overview_label);
    gtk_box_append(GTK_BOX(dashboard_box), overview_box);
    gtk_box_append(GTK_BOX(left_panel), dashboard_frame);

    GtkWidget *process_frame = gtk_frame_new("Process List");
    GtkWidget *process_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_frame_set_child(GTK_FRAME(process_frame), process_box);

    GtkWidget *process_header = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(process_header), 5);
    gtk_grid_attach(GTK_GRID(process_header), gtk_label_new("PID"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(process_header), gtk_label_new("State"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(process_header), gtk_label_new("Priority"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(process_header), gtk_label_new("PC"), 3, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(process_header), gtk_label_new("Memory"), 4, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(process_header), gtk_label_new("Time in Queue"), 5, 0, 1, 1);
    gtk_box_append(GTK_BOX(process_box), process_header);

    process_store = g_list_store_new(GTK_TYPE_STRING_OBJECT);
    GtkSelectionModel *process_selection = GTK_SELECTION_MODEL(gtk_single_selection_new(G_LIST_MODEL(process_store)));
    
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_list_item), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_list_item), NULL);
    
    process_list_view = gtk_list_view_new(process_selection, factory);
    GtkWidget *process_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(process_scroll), process_list_view);
    gtk_widget_set_size_request(process_scroll, -1, 200);
    gtk_box_append(GTK_BOX(process_box), process_scroll);
    gtk_box_append(GTK_BOX(left_panel), process_frame);

    GtkWidget *queue_frame = gtk_frame_new("Queue Status");
    GtkWidget *queue_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_frame_set_child(GTK_FRAME(queue_frame), queue_box);

    GtkWidget *queue_header = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(queue_header), 5);
    gtk_grid_attach(GTK_GRID(queue_header), gtk_label_new("Queue Type"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(queue_header), gtk_label_new("PID"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(queue_header), gtk_label_new("Time in Queue"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(queue_header), gtk_label_new("Priority"), 3, 0, 1, 1);
    gtk_box_append(GTK_BOX(queue_box), queue_header);

    queue_store = g_list_store_new(GTK_TYPE_STRING_OBJECT);
    GtkSelectionModel *queue_selection = GTK_SELECTION_MODEL(gtk_single_selection_new(G_LIST_MODEL(queue_store)));
    
    GtkListItemFactory *queue_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(queue_factory, "setup", G_CALLBACK(setup_list_item), NULL);
    g_signal_connect(queue_factory, "bind", G_CALLBACK(bind_list_item), NULL);
    
    queue_list_view = gtk_list_view_new(queue_selection, queue_factory);
    GtkWidget *queue_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(queue_scroll), queue_list_view);
    gtk_widget_set_size_request(queue_scroll, -1, 200);
    gtk_box_append(GTK_BOX(queue_box), queue_scroll);
    gtk_box_append(GTK_BOX(left_panel), queue_frame);

    GtkWidget *resource_frame = gtk_frame_new("Resource Management");
    GtkWidget *resource_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_frame_set_child(GTK_FRAME(resource_frame), resource_box);

    mutex_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(mutex_grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(mutex_grid), 10);
    gtk_widget_set_margin_start(mutex_grid, 10);
    gtk_widget_set_margin_end(mutex_grid, 10);
    gtk_widget_set_margin_top(mutex_grid, 10);
    gtk_widget_set_margin_bottom(mutex_grid, 10);

    GtkWidget *resource_header = gtk_label_new("Resource");
    GtkWidget *status_header = gtk_label_new("Status");
    GtkWidget *owner_header = gtk_label_new("Owner PID");
    gtk_widget_add_css_class(resource_header, "resource-header");
    gtk_widget_add_css_class(status_header, "resource-header");
    gtk_widget_add_css_class(owner_header, "resource-header");
    
    gtk_grid_attach(GTK_GRID(mutex_grid), resource_header, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(mutex_grid), status_header, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(mutex_grid), owner_header, 2, 0, 1, 1);

    const char *resources[] = {"User Input", "File", "User Output"};
    for (int i = 0; i < 3; i++) {
        GtkWidget *label = gtk_label_new(resources[i]);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(mutex_grid), label, 0, i + 1, 1, 1);
    }

    gtk_box_append(GTK_BOX(resource_box), mutex_grid);
    gtk_box_append(GTK_BOX(left_panel), resource_frame);

    sim_state.clockCycle = 0;

    GtkWidget *control_frame = gtk_frame_new("Scheduler Control");
    GtkWidget *control_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_frame_set_child(GTK_FRAME(control_frame), control_box);

    GtkWidget *scheduler_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(scheduler_box), gtk_label_new("Scheduler:"));
    GtkStringList *scheduler_list = gtk_string_list_new(NULL);
    gtk_string_list_append(scheduler_list, "mlfq");
    gtk_string_list_append(scheduler_list, "rr");
    gtk_string_list_append(scheduler_list, "fcfs");
    scheduler_dropdown = gtk_drop_down_new(G_LIST_MODEL(scheduler_list), NULL);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(scheduler_dropdown), 0);
    g_signal_connect(scheduler_dropdown, "notify::selected", G_CALLBACK(on_scheduler_changed), NULL);
    gtk_box_append(GTK_BOX(scheduler_box), scheduler_dropdown);
    gtk_box_append(GTK_BOX(control_box), scheduler_box);

    GtkWidget *quantum_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(quantum_box), gtk_label_new("Quantum:"));
    quantum_spin = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(quantum_spin), sim_state.rrQuantum);
    g_signal_connect(quantum_spin, "value-changed", G_CALLBACK(on_quantum_changed), NULL);
    gtk_box_append(GTK_BOX(quantum_box), quantum_spin);
    gtk_box_append(GTK_BOX(control_box), quantum_box);

    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    start_button = gtk_button_new_with_label("Start");
    stop_button = gtk_button_new_with_label("Stop");
    reset_button = gtk_button_new_with_label("Reset");
    step_button = gtk_button_new_with_label("Step");
    auto_button = gtk_button_new_with_label("Auto Execute");

    g_signal_connect(start_button, "clicked", G_CALLBACK(on_start_clicked), NULL);
    g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop_clicked), NULL);
    g_signal_connect(reset_button, "clicked", G_CALLBACK(on_reset_clicked), NULL);
    g_signal_connect(step_button, "clicked", G_CALLBACK(on_step_clicked), NULL);
    g_signal_connect(auto_button, "clicked", G_CALLBACK(on_auto_clicked), NULL);

    gtk_box_append(GTK_BOX(button_box), start_button);
    gtk_box_append(GTK_BOX(button_box), stop_button);
    gtk_box_append(GTK_BOX(button_box), reset_button);
    gtk_box_append(GTK_BOX(button_box), step_button);
    gtk_box_append(GTK_BOX(button_box), auto_button);
    gtk_box_append(GTK_BOX(control_box), button_box);

    GtkWidget *process_control_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    add_process_button = gtk_button_new_with_label("Add Process");
    arrival_spin = gtk_spin_button_new_with_range(0, 1000, 1);
    gtk_box_append(GTK_BOX(process_control_box), gtk_label_new("Arrival Time:"));
    gtk_box_append(GTK_BOX(process_control_box), arrival_spin);
    gtk_box_append(GTK_BOX(process_control_box), add_process_button);
    g_signal_connect(add_process_button, "clicked", G_CALLBACK(on_add_process_clicked), NULL);
    gtk_box_append(GTK_BOX(control_box), process_control_box);

    gtk_box_append(GTK_BOX(right_top), control_frame);

    GtkWidget *log_frame = gtk_frame_new("Execution Log");
    GtkWidget *log_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_frame_set_child(GTK_FRAME(log_frame), log_box);

    log_text_view = gtk_text_view_new();
    gtk_widget_add_css_class(log_text_view, "log-text-large");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(log_text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(log_text_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(log_text_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(log_text_view), GTK_WRAP_WORD_CHAR);
    
    GtkWidget *log_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(log_scroll),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(log_scroll, TRUE);
    gtk_widget_set_hexpand(log_scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(log_scroll), log_text_view);
    
    gtk_box_append(GTK_BOX(log_box), log_scroll);

    log_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(log_entry), "Type command and press Enter...");
    g_signal_connect(log_entry, "activate", G_CALLBACK(on_log_entry_activate), NULL);
    gtk_box_append(GTK_BOX(log_box), log_entry);

    gtk_box_append(GTK_BOX(right_bottom), log_frame);

    setup_memory_grid_viewer(right_bottom);

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_text_view));
    gtk_text_buffer_set_text(buffer, "Scheduler Simulation Log Started...\n", -1);

    gtk_widget_set_vexpand(log_frame, TRUE);

    gtk_window_present(GTK_WINDOW(window));
}

void init_gui(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new("org.os.scheduler", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(setup_gui), NULL);
    g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
}