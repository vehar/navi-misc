from django.conf.urls.defaults import *

urlpatterns = patterns('',
        (r'^site_media/(?P<path>.*)$', 'django.views.static.serve',
            {'document_root': 'site_media'}),
        (r'^admin/', include('django.contrib.admin.urls')),
)

# vim: ts=4:sw=4:et
