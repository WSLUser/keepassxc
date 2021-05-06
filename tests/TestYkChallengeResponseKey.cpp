/*
 *  Copyright (C) 2014 Kyle Manna <kyle@kylemanna.com>
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
 *
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "TestYkChallengeResponseKey.h"
#include "TestGlobal.h"

#include "core/Tools.h"
#include "crypto/Crypto.h"
#include "keys/YkChallengeResponseKey.h"

#include <QScopedPointer>
#include <QSignalSpy>

QTEST_GUILESS_MAIN(TestYubiKeyChallengeResponse)

void TestYubiKeyChallengeResponse::initTestCase()
{
    // crypto subsystem needs to be initialized for YubiKey testing
    QVERIFY(Crypto::init());

    if (!YubiKey::instance()->isInitialized()) {
        QSKIP("Unable to initialize YubiKey interface.");
    }
}

void TestYubiKeyChallengeResponse::testDetectDevices()
{
    YubiKey::instance()->findValidKeys();

    // Wait for the hardware to respond
    QSignalSpy detected(YubiKey::instance(), SIGNAL(detectComplete(bool)));
    QTRY_VERIFY_WITH_TIMEOUT(detected.count() > 0, 2000);

    // Look at the information retrieved from the key(s)
    for (auto key : YubiKey::instance()->foundKeys()) {
        auto displayName = YubiKey::instance()->getDisplayName(key);
        QVERIFY(displayName.contains("Challenge Response - Slot") || displayName.contains("Configured Slot -"));
        QVERIFY(displayName.contains(QString::number(key.first)));
        QVERIFY(displayName.contains(QString::number(key.second)));
    }
}

/**
 * Secret key for the YubiKey slot used by the unit test is
 * 1c e3 0f d7 8d 20 dc fa 40 b5 0c 18 77 9a fb 0f 02 28 8d b7
 * This secret can be on either slot but must be passive.
 */
void TestYubiKeyChallengeResponse::testKeyChallenge()
{
    auto keys = YubiKey::instance()->foundKeys();
    if (keys.isEmpty()) {
        QSKIP("No YubiKey devices were detected.");
    }

    // Find a key that is configured in passive mode
    bool wouldBlock = false;
    YubiKeySlot pKey(0, 0);
    for (auto key : keys) {
        if (YubiKey::instance()->testChallenge(key, &wouldBlock) && !wouldBlock) {
            pKey = key;
            break;
        }
        Tools::wait(100);
    }

    if (pKey.first == 0) {
        /* Testing active mode in unit tests is unreasonable */
        QSKIP("No YubiKey contains a slot in passive mode.");
    }

    QScopedPointer<YkChallengeResponseKey> key(new YkChallengeResponseKey(pKey));

    QByteArray ba("UnitTest");
    QVERIFY(key->challenge(ba));
    QCOMPARE(key->rawKey().size(), 20UL);
}
