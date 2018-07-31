# nginx-mysql-module

(c) 2012 Arutyunyan Roman (arut@qip.ru)

## MySQL support in NGINX

* this module is asynchronous (non-blocking)
* requires nginx-mtask-module (https://github.com/arut/nginx-mtask-module)
* requires standard MySQL client shared library libmysqlclient.so


NGINX configure:

```bash
./configure --add-module=/home/rarutyunyan/nginx-mtask-module --add-module=/home/rarutyunyan/nginx-mysql-module
```

Usage examples:

MySQL:

```mysql
USE test;

CREATE TABLE `users` (
  `name` varchar(128) DEFAULT NULL,
  `id` int(11) NOT NULL auto_increment,  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
```

nginx.conf:

```nginx
http {

  # ...

  server {

    # ...

    # TCP/IP access
    #mysql_host 127.0.0.1;

    # unix socket access (default)
    #mysql_host localhost;

    #mysql_user theuser;
    #mysql_password thepass;
    #mysql_port theport;

    mysql_database test;
    mysql_connections 32;

    mysql_charset utf8;

    # this turns on multiple statements support
    # and is REQUIRED when calling stored procedures
    mysql_multi on;

    mtask_stack 65536;


    location ~ /all {

      mysql_query "SELECT name FROM users WHERE id='$arg_id'";
    }

    location /multi {

      mysql_query "SELECT id, name FROM users ORDER BY id DESC LIMIT 10; SELECT id, name FROM users ORDER BY id ASC LIMIT 10";
    }

    location ~ /select.* {

      if ( $arg_id ~* ^\d+$ ) {

        mysql_query "SELECT name FROM users WHERE id='$arg_id'";

      }
    }

    location ~ /insert.* {

      mysql_escape $escaped_name $arg_name;

      mysql_query "INSERT INTO users(name) VALUES('$escaped_name')";
    }

    location ~ /update.* {

      mysql_query "UPDATE users SET name='$arg_name' WHERE id=$arg_id";
    }

    location ~ /delete.* {

      mysql_query "DELETE FROM users WHERE id=$arg_id";
    }

    # store number of page shows per user
    location ~ /register-by-cookie-show.* {

      # register show in database
      # need a location with this query:
      # mysql_query "INSERT INTO shows(id) VALUES('$id') ON DUPLICATE KEY UPDATE count=count+1";
      mysql_subrequest /update_shows_in_mysql?id=$cookie_abcdef;

      # output content
      rewrite ^(.*)$ /content last;
    }

    # spammers use big ids in picture refs to see who opened the message
    location ~/spammer-style-image-show-register.* {

      # need a location with this query:
      # mysql_query "INSERT INTO shows(id) VALUES('$arg_id')";
      mysql_subrequest /insert_show?id=$arg_id;

      rewrite ^(.*)$ /content last;
    }

    location ~ /session.* {

      # get user session
      # need a location with this query:
      # mysql_query "SELECT username, lastip FROM sessions WHERE id='$id'";
      mysql_subrequest /select_from_mysql?id=$cookie_abcdef $username $lastip;

      # go to content generator
      rewrite ^(.*)$ /content?username=$username&lastip=$lastip last;
    }

    # ...
  }

  # ...

}
```

access:

```
curl 'localhost:8080/insert?name=foo'
11

curl 'localhost:8080/select?id=11'
foo

curl 'localhost:8080/update?id=11&name=bar'
<nothing>

curl 'localhost:8080/select?id=11'
bar

curl 'localhost:8080/delete?id=11'
<nothing>

curl 'localhost:8080/select?id=11'
<nothing>
```
