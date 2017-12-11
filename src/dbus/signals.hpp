//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Affero General Public License as
//  published by the Free Software Foundation, version 3 of the
//  License.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Affero General Public License for more details.
//
//  You should have received a copy of the GNU Affero General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#ifndef OPENVPN3_DBUS_SIGNALS_HPP
#define OPENVPN3_DBUS_SIGNALS_HPP

#include <map>

namespace openvpn
{
    inline const char * string2C_char(std::string in)
    {
            return (in.empty() ? NULL : in.c_str());
    }


    inline std::string C_char2string(const char *in)
    {
            return (NULL == in ? "" : std::string(in));
    }


    class DBusSignalSubscription
    {
    public:
        // FIXME:  Reduce number of constructors
        //         Move towards passing these variables to Subscribe()
        //         as the normal way to subscribe

        DBusSignalSubscription(DBus & dbusobj,
                               std::string interf)
            : conn(dbusobj.GetConnection()),
              interface(interf)
        {
        }


        DBusSignalSubscription(DBus & dbusobj,
                               std::string busname,
                               std::string interf,
                               std::string objpath)
            : conn(dbusobj.GetConnection()),
              bus_name(busname),
              interface(interf),
              object_path(objpath)
        {
        }


        DBusSignalSubscription(GDBusConnection *dbuscon,
                               std::string interf)
            : conn(dbuscon),
              interface(interf)
        {
        }


        DBusSignalSubscription(GDBusConnection *dbuscon,
                               std::string busname,
                               std::string interf,
                               std::string objpath)
            : conn(dbuscon),
              bus_name(busname),
              interface(interf),
              object_path(objpath)
        {
        }


        DBusSignalSubscription(DBus & dbusobj,
                               std::string busname,
                               std::string interf,
                               std::string objpath,
                               std::string signame)
            : conn(dbusobj.GetConnection()),
              bus_name(busname),
              interface(interf),
              object_path(objpath)
        {
            Subscribe(signame);
        }


        DBusSignalSubscription(GDBusConnection *dbuscon,
                               std::string busname,
                               std::string interf,
                               std::string objpath,
                               std::string signame)
            : conn(dbuscon),
              bus_name(busname),
              interface(interf),
              object_path(objpath)
        {
            Subscribe(signame);
        }


        /**
         *  If another destructor overrides this one,
         *  the caller *MUST* run Cleanup() before exiting
         *  to properly unsibscribe from the D-Bus signal
         */
        virtual ~DBusSignalSubscription()
        {
            Cleanup();
        }

        /**
         *  Called each time the subscribed signal has a match on the D-Bus
         */
        virtual void callback_signal_handler(GDBusConnection *connection,
                                             const std::string sender_name,
                                             const std::string object_path,
                                             const std::string interface_name,
                                             const std::string signal_name,
                                             GVariant *parameters) = 0;


        void Subscribe(std::string busname, std::string objpath, std::string signal_name)
        {
            guint signal_id = g_dbus_connection_signal_subscribe(conn,
                                                           string2C_char(busname),
                                                           string2C_char(interface),
                                                           string2C_char(signal_name),
                                                           string2C_char(objpath),
                                                           NULL,
                                                           G_DBUS_SIGNAL_FLAGS_NONE,
                                                           dbusobject_callback_signal_handler,
                                                           this,
                                                           NULL );  // destructor
            if (signal_id < 0)
            {
                std::stringstream err;
                err << "Failed to subscribe to the " << signal_name << "signal on"
                    << object_path << "[" << interface << "]";
                THROW_DBUSEXCEPTION("DBusSignalSubscription", err.str());
            }
            subscriptions[signal_name] = signal_id;
            subscribed = true;
        }


        void Subscribe(std::string objpath, std::string signal_name)
        {
            Subscribe(bus_name, objpath, signal_name);
        }


        void Subscribe(std::string signal_name)
        {
            Subscribe(object_path, signal_name);
        }


        void Unsubscribe(std::string signal_name)
        {
            if (subscriptions[signal_name] > 0)
            {
                g_dbus_connection_signal_unsubscribe(conn, subscriptions[signal_name]);
                subscriptions[signal_name] = 0;
            }
        }


        guint GetSignalId(std::string signal_name)
        {
            return subscriptions[signal_name];
        }


        void Cleanup()
        {
            if( !subscribed )
            {
                return;
            }
            for( auto& sub : subscriptions)
            {
                if (sub.second > 0)
                {
                    g_dbus_connection_signal_unsubscribe(conn, sub.second);
                }
                subscriptions[sub.first] = 0;
            }
            subscribed = false;
        }


    protected:
        GDBusConnection * GetConnection()
        {
            return conn;
        }


        static void dbusobject_callback_signal_handler(GDBusConnection *conn,
                                                        const gchar *sender,
                                                        const gchar *obj_path,
                                                        const gchar *intf_name,
                                                        const gchar *sign_name,
                                                        GVariant *params,
                                                        gpointer this_ptr)
        {
            class DBusSignalSubscription *obj = (class DBusSignalSubscription *) this_ptr;
            obj->callback_signal_handler(conn,
                                         std::string(sender),
                                         std::string(obj_path),
                                         std::string(intf_name),
                                         std::string(sign_name),
                                         params);
        }


    private:
        GDBusConnection *conn;
        std::string bus_name;
        std::string interface;
        std::string object_path;
        std::map<std::string, guint> subscriptions;
        bool subscribed;
    };



    class DBusSignalProducer
    {
    public:
        DBusSignalProducer(DBus dbuscon,
                           std::string busname,
                           std::string interf,
                           std::string objpath) :
            conn(dbuscon.GetConnection()),
            bus_name(busname),
            interface(interf),
            object_path(objpath)
        {
            validate_params();
        }


        DBusSignalProducer(GDBusConnection *con,
                           std::string busname,
                           std::string interf,
                           std::string objpath) :
            conn(con),
            bus_name(busname),
            interface(interf),
            object_path(objpath)
        {
            validate_params();
        }


        void Send(const std::string busn,
                  const std::string interf,
                  const std::string objpath,
                  const std::string signal_name,
                  GVariant *params)
        {
            /*
              std::cout << "Signal Send: bus=" << (!bus_name.empty() ? bus_name : "(not set)")
                      << ", interface=" << (!interface.empty() ? interface : "(not set)")
                      << ", object_path=" << (!object_path.empty() ? object_path : "(not set)")
                      << ", signal_name=" << signal_name
                      << std::endl;
            */
            GError *error = NULL;

            if( !g_dbus_connection_emit_signal(conn,
                                               string2C_char(busn),
                                               string2C_char(objpath),
                                               string2C_char(interf),
                                               signal_name.c_str(),
                                               params,
                                               &error))
            {
                std::stringstream errmsg;
                errmsg << "Failed to send '" + signal_name + "' signal";

                if (error)
                {
                    errmsg << ": " << error->message;
                }
                THROW_DBUSEXCEPTION("DBusSignalProducer", errmsg.str());
            }
        }


        void Send(const std::string busn,
                  const std::string interf,
                  const std::string signal_name,
                  GVariant *params)
        {
            Send(busn, interf, object_path, signal_name, params);
        }


        void Send(const std::string busn, const std::string interf, const std::string signal_name)
        {
            Send(busn, interf, object_path, signal_name, NULL);
        }


        void Send(const std::string interf, const std::string signal_name, GVariant *params)
        {
            Send(bus_name, interf, object_path, signal_name, params);
        }


        void Send(const std::string interf, const std::string signal_name)
        {
            Send(bus_name, interf, object_path, signal_name, NULL);
        }


        void Send(const std::string signal_name, GVariant *params)
        {
            Send(bus_name, interface, object_path, signal_name, params);
        }


        void Send(const std::string signal_name)
        {
            Send(bus_name, interface, object_path, signal_name, NULL);
        }


    protected:
        void validate_params()
        {
            if (interface.empty()) {
                THROW_DBUSEXCEPTION("DBusSignalProducer", "Interface cannot be empty");
            }

            if (object_path.empty()) {
                THROW_DBUSEXCEPTION("DBusSignalProducer", "Object path cannot be empty");
            }
        }


    private:
        GDBusConnection *conn;
        std::string bus_name;
        std::string interface;
        std::string object_path;
        std::string signal_name;
    };
};
#endif // OPENVPN3_DBUS_SIGNALS_HPP
