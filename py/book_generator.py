import base64
import pickle
import gzip

combined = {}

# with open('book_backup.pk', 'rb') as f:
#     data = pickle.load(f)
#     print(f'book has {len(data)} positions')
#     combined = {**combined, **data}

with open('backup_book_red8.pk', 'rb') as f:
    data = pickle.load(f)
    print(f'book red8 has {len(data)} positions')
    combined = {**combined, **data}

with open('backup_book_blue8.pk', 'rb') as f:
    data = pickle.load(f)
    print(f'book blue8 has {len(data)} positions')
    combined = {**combined, **data}

# with open('backup_book_red.pk', 'rb') as f:
#     data = pickle.load(f)
#     print(f'book red has {len(data)} positions')
#     combined = {**combined, **data}
#
# with open('backup_book_blue.pk', 'rb') as f:
#     data = pickle.load(f)
#     print(f'book blue has {len(data)} positions')
#     combined = {**combined, **data}


with open('backup_book_red1s.pk', 'rb') as f:
    data = pickle.load(f)
    print(f'book red1s has {len(data)} positions')
    combined = {**combined, **data}

with open('backup_book_blue1s.pk', 'rb') as f:
    data = pickle.load(f)
    print(f'book blue1s has {len(data)} positions')
    combined = {**combined, **data}


print(f'total {len(combined)} positions')
text = base64.b64encode(gzip.compress(pickle.dumps(combined)))
with open('book_base64', 'w') as f:
    f.write(text.decode('ascii'))
